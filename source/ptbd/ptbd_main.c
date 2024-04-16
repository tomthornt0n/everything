
#include "external/sqlite3.h"
#include "external/libxlsxwriter/xlsxwriter.h"
#include "external/fonts.h"

#include "base/base_incl.h"
#include "os/os_incl.h"
#include "graphics/graphics_incl.h"
#include "renderer_2d/renderer_2d_incl.h"
#include "font/font_incl.h"
#include "draw/draw_incl.h"
#include "ui/ui_incl.h"
#include "ui/ui_widgets.h"

#include "base/base_incl.c"
#include "os/os_incl.c"
#include "graphics/graphics_incl.c"
#include "renderer_2d/renderer_2d_incl.c"
#include "font/font_incl.c"
#include "draw/draw_incl.c"
#include "ui/ui_incl.c"
#include "ui/ui_widgets.c"

//~

Function void PTBD_WindowHookOpen  (OpaqueHandle window);
Function void PTBD_WindowHookDraw  (OpaqueHandle window);
Function void PTBD_WindowHookClose (OpaqueHandle window);
ReadOnlyVar Global G_WindowHooks ptbd_window_hooks =
{
    PTBD_WindowHookOpen,
    PTBD_WindowHookDraw,
    PTBD_WindowHookClose,
};

#include "ptbd_selection.h"
#include "ptbd_db.h"
#include "ptbd_index.h"
#include "ptbd_drag_drop.h"

typedef struct PTBD_CriticalState PTBD_CriticalState;
struct PTBD_CriticalState
{
    DB_Cache cache[DB_Table_MAX];
    PTBD_Index timeline_histogram_index;
    PTBD_Index invoicing_histogram_index;
    PTBD_Index jobs_list_index;
    PTBD_Index invoices_list_index;
    PTBD_Index employees_list_index;
    S8 search;
    char search_buffer[1024];
};

typedef struct PTBD_ArenasForCriticalState PTBD_ArenasForCriticalState;
struct PTBD_ArenasForCriticalState
{
    Arena *cache_arena;
    Arena *view_arena;
};

Global OpaqueHandle ptbd_gradient_vertical = {0};
Global OpaqueHandle ptbd_gradient_horizontal = {0};

#include "ptbd_messages.h"
#include "ptbd_empty_screen.h"
#include "ptbd_inspector.h"
#include "ptbd_undo.h"
#include "ptbd_histogram.h"
#include "ptbd_jobs_list.h"
#include "ptbd_invoices_list.h"
#include "ptbd_employees_list.h"

Function UI_Use PTBD_BuildJobEmblem      (DB_Row *job, B32 should_include_name);
Function UI_Use PTBD_BuildEmployeeEmblem (DB_Row *employee, B32 should_include_name);
Function UI_Use PTBD_BuildInvoiceEmblem  (DB_Row *invoice);

Function void PTBD_CopyIndex         (Arena *arena, PTBD_Index *dst, PTBD_Index *src);
Function void PTBD_CopyDBCache       (Arena *arena, DB_Cache *dst, DB_Cache *src);
Function void PTBD_CopyCriticalState (Arena *arena, PTBD_CriticalState *dst, PTBD_CriticalState *src);

Function void PTBD_Reload  (Arena *arena, sqlite3 *db, DB_Cache caches[DB_Table_MAX]);
Function void PTBD_Refresh (Arena *arena, PTBD_CriticalState *state);

Function S8 PTBD_DBConfigFilename (Arena *arena);
Function S8 PTBD_DBFilename       (Arena *arena);

Function B32 PTBD_SearchHasRow (S8 search, DB_Row *row);

Function PTBD_IndexCoord PTBD_IndexCoordFromDBId (PTBD_Index *index, DB_Cache data, I64 id);

Function V4F PTBD_ColourFromInvoiceId  (I64 invoice_id);
Function V4F PTBD_ColourFromEmployeeId (I64 employee_id);

Function UI_Window PTBD_UIWindowFromContainerScrollHeightPerRowAndRowsCount (UI_Node *container, F32 height_per_row, U64 rows_count, F32 vertical_scroll);

Function void PTBD_ExcelFileFromInvoice (DB_Row *invoice, S8 filename);

Global OpaqueHandle ptbd_scroll_to_lock;
Global I64 ptbd_next_scroll_to[DB_Table_MAX] = {0};
Global I64 ptbd_scroll_to[DB_Table_MAX] = {0};
Global B32 ptbd_did_scroll_to[DB_Table_MAX] = {0};
Function void PTBD_ScrollToRow   (DB_Table table, I64 id);
Function void PTBD_ApplyScrollToForList      (DB_Table table, DB_Cache data, DB_Cache timesheets, PTBD_Index *index, PTBD_Selection *expanded, F32 *scroll, F32 window_size, F32 height_per_row, U64 records_per_row);
Function void PTBD_ApplyScrollToForHistogram (DB_Cache timesheets, PTBD_Index *index, F32 *scroll, F32 window_size, F32 height_per_row);

typedef enum
{
    PTBD_Zoom_Overview,
    PTBD_Zoom_Timeline,
    PTBD_Zoom_Jobs,
    PTBD_Zoom_Invoicing,
    PTBD_Zoom_Invoices,
    PTBD_Zoom_Employees,
} PTBD_Zoom;

typedef struct PTBD_Application PTBD_Application;
struct PTBD_Application
{
    Arena *permanent_arena;
    
    UI_State *ui_state;
    
    sqlite3 *db;
    
    B32 volatile is_running;
    
    PTBD_MsgQueue m2c;
    PTBD_MsgQueue c2m;
    
    OpaqueHandle state_lock; // NOTE(tbt): locked when writing to next_state
    B32 volatile is_next_state_dirty; // NOTE(tbt): used by commands thread, set to true when next_state has diverged from state
    B32 volatile should_copy_state; // NOTE(tbt): set to true by commands thread when reaching the end of a 'burst' of commands, before sleeping, if is_next_state_dirty
    Arena *state_arena; // NOTE(tbt): used to allocate the copy of state - user thread only needs a single arena as the whole state is copied in one go
    PTBD_CriticalState state; // NOTE(tbt): the critical state for the application
    PTBD_ArenasForCriticalState next_state_arenas; // NOTE(tbt): used by the commands thread while mutating next_state - needs multiple arenas as different members have different lifetimes
    PTBD_CriticalState next_state; // NOTE(tbt): the version of the app's critical state which is mutated by the commands thread - the user thread copies this when necessary
    F32 is_loading_t;
    
    S8Builder search_bar;
    
    PTBD_Histogram timeline_view;
    PTBD_Histogram invoicing_view;
    
    PTBD_Selection *selected_timesheets;
    
    PTBD_JobsListRowId job_keyboard_focus_id;
    I64 job_last_selected_id;
    PTBD_Selection *expanded_jobs;
    PTBD_JobEditor job_editor;
    
    I64 invoice_keyboard_focus_id;
    PTBD_InvoiceEditor invoice_editor;
    
    I64 employee_keyboard_focus_id;
    PTBD_EmployeeEditor employee_editor;
    
    B32 timesheet_inspectors_have_errors;
    PTBD_Inspectors timesheet_inspectors;
    
    PTBD_Zoom zoom;
};

Global F32 ptbd_is_dark_mode_t = 0.0f;

Global V4F ptbd_grey = {0};

//~

#include "ptbd_db.c"
#include "ptbd_messages.c"
#include "ptbd_index.c"
#include "ptbd_selection.c"
#include "ptbd_drag_drop.c"
#include "ptbd_histogram.c"
#include "ptbd_jobs_list.c"
#include "ptbd_invoices_list.c"
#include "ptbd_employees_list.c"
#include "ptbd_empty_screen.c"
#include "ptbd_inspector.c"
#include "ptbd_undo.c"

EntryPoint
{
    // NOTE(tbt): dependency layers init
    OS_Init();
    G_Init();
    R2D_Init();
    FONT_Init();
    
    Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
    
    {
        Pixel pixels[128] = {0};
        for(U64 pixel_index = 0; pixel_index < ArrayCount(pixels); pixel_index += 1)
        {
            //U8 f = (255*pixel_index) / ArrayCount(pixels);
            U8 f = 255.0f*LinearFromSRGB((F32)pixel_index / ArrayCount(pixels));
            pixels[pixel_index].r = f;
            pixels[pixel_index].g = f;
            pixels[pixel_index].b = f;
            pixels[pixel_index].a = f;
        }
        ptbd_gradient_vertical = R2D_TextureAlloc(pixels, V2I(1, ArrayCount(pixels)));
        ptbd_gradient_horizontal = R2D_TextureAlloc(pixels, V2I(ArrayCount(pixels), 1));
    }
    ptbd_drag_drop.arena = ArenaAlloc(Kilobytes(16));
    ptbd_undo_lock = OS_SemaphoreAlloc(1);
    ptbd_undo_arena = ArenaAlloc(Gigabytes(2));
    ptbd_redo_arena = ArenaAlloc(Gigabytes(2));
    ptbd_scroll_to_lock = OS_SemaphoreAlloc(1);
    
    PTBD_Application *app = PushArray(permanent_arena, PTBD_Application, 1);
    
    OpaqueHandle window = G_WindowOpen(S8("PTBD Database"), V2I(1024, 768), ptbd_window_hooks, app);
    
    app->permanent_arena = permanent_arena;
    app->ui_state = UI_Init(window);
    app->timeline_view.height_per_row = 85.0f;
    app->timeline_view.width_per_hour = 40.0f;
    app->timeline_view.leader_column_width = 150.0f;
    app->invoicing_view.height_per_row = 64.0f;
    app->invoicing_view.width_per_hour = 40.0f;
    app->invoicing_view.leader_column_width = 150.0f;
    app->m2c.sem = OS_SemaphoreAlloc(0);
    app->m2c.lock = OS_SemaphoreAlloc(1);
    app->c2m.lock = OS_SemaphoreAlloc(1);
    app->state_lock = OS_SemaphoreAlloc(1);
    app->state_arena = ArenaAlloc(Gigabytes(1));
    app->next_state_arenas.cache_arena = ArenaAlloc(Gigabytes(2));
    app->next_state_arenas.view_arena = ArenaAlloc(Gigabytes(1));
    app->search_bar.cap = 1024;
    app->search_bar.buffer = PushArray(permanent_arena, char, app->search_bar.cap);
    app->job_editor.id = -1;
    app->job_editor.number.cap = 8;
    app->job_editor.number.buffer = PushArray(permanent_arena, char, app->job_editor.number.cap);
    app->job_editor.name.cap = 64;
    app->job_editor.name.buffer = PushArray(permanent_arena, char, app->job_editor.name.cap);
    app->invoice_editor.title.cap = 64;
    app->invoice_editor.title.buffer = PushArray(permanent_arena, char, app->invoice_editor.title.cap);
    app->invoice_editor.address.cap = 512;
    app->invoice_editor.address.buffer = PushArray(permanent_arena, char, app->invoice_editor.address.cap);
    app->invoice_editor.description.cap = 1024;
    app->invoice_editor.description.buffer = PushArray(permanent_arena, char, app->invoice_editor.description.cap);
    app->invoice_editor.cost.cap = 12;
    app->invoice_editor.cost.buffer = PushArray(permanent_arena, char, app->invoice_editor.cost.cap);
    app->employee_editor.id = -1;
    app->employee_editor.name.cap = 512;
    app->employee_editor.name.buffer = PushArray(permanent_arena, char, app->employee_editor.name.cap);
    app->employee_editor.rate.cap = 12;
    app->employee_editor.rate.buffer = PushArray(permanent_arena, char, app->employee_editor.rate.cap);
    app->employee_keyboard_focus_id = -1;
    U64 default_selection_chunk_cap = 32;
    app->selected_timesheets = ArenaPush(permanent_arena, sizeof(*app->selected_timesheets) + default_selection_chunk_cap*sizeof(app->selected_timesheets->ids[0]), _Alignof(PTBD_Selection));
    app->selected_timesheets->cap = default_selection_chunk_cap;
    app->expanded_jobs = ArenaPush(permanent_arena, sizeof(*app->expanded_jobs) + default_selection_chunk_cap*sizeof(app->expanded_jobs->ids[0]), _Alignof(PTBD_Selection));
    app->expanded_jobs->cap = default_selection_chunk_cap;
    
    // NOTE(tbt): graphics setup
    G_ShouldWaitForEvents(True);
    G_WindowVSyncSet(window, True);
    
    // NOTE(tbt): command processing thread main loop
    OpaqueHandle commands_thread = OS_ThreadStart(PTBD_CmdThreadProc, app);
    M2C_Reload(&app->m2c);
    
    // NOTE(tbt): user thread main loop
    G_MainLoop();
    
    // NOTE(tbt): join other threads
    OS_SemaphoreSignal(app->m2c.sem);
    OS_ThreadJoin(commands_thread);
    
    // NOTE(tbt): dependency layers cleanup
    R2D_Cleanup();
    G_Cleanup();
    OS_Cleanup();
    
    return 0;
}

Function UI_Use
PTBD_BuildEmployeeEmblem(DB_Row *employee, B32 should_include_name)
{
    UI_SetNextWidth(UI_SizeSum(should_include_name ? 0.5f : 1.0f));
    UI_SetNextHeight(UI_SizeSum(1.0f));
    UI_SetNextBgCol(PTBD_ColourFromEmployeeId(employee->id));
    UI_SetNextCornerRadius(12.0f);
    UI_SetNextChildrenLayout(Axis2_X);
    UI_Node *employee_emblem =
        UI_NodeMakeFromFmt(UI_Flags_DrawFill |
                           UI_Flags_DrawDropShadow |
                           UI_Flags_DrawHotStyle |
                           UI_Flags_DrawActiveStyle |
                           UI_Flags_AnimateHot |
                           UI_Flags_AnimateActive |
                           UI_Flags_AnimateSmoothLayout |
                           UI_Flags_Pressable,
                           "employee emblem %lld",
                           employee->id);
    UI_Parent(employee_emblem) UI_Pad(UI_SizePx(4.0f, 1.0f))
    {
        UI_LabelFromIcon(FONT_Icon_Adult);
        if(should_include_name)
        {
            UI_TextTruncSuffix(S8("...")) UI_Label(employee->employee_name);
        }
    }
    UI_Use use = UI_UseFromNode(employee_emblem);
    if(use.is_dragging && 0 == ptbd_drag_drop.count)
    {
        PTBD_DragRow(DB_Table_Employees, *employee);
    }
    return use;
}

Function UI_Use
PTBD_BuildJobEmblem(DB_Row *job, B32 should_include_name)
{
    UI_SetNextWidth(UI_SizeSum(0.5f));
    UI_SetNextHeight(UI_SizeSum(1.0f));
    UI_SetNextCornerRadius(12.0f);
    UI_SetNextChildrenLayout(Axis2_X);
    UI_Node *employee_emblem =
        UI_NodeMakeFromFmt(UI_Flags_DrawFill |
                           UI_Flags_DrawDropShadow |
                           UI_Flags_DrawHotStyle |
                           UI_Flags_DrawActiveStyle |
                           UI_Flags_AnimateHot |
                           UI_Flags_AnimateActive |
                           UI_Flags_Pressable,
                           "job emblem %lld",
                           job->id);
    UI_Parent(employee_emblem) UI_Pad(UI_SizePx(4.0f, 1.0f))
    {
        UI_Font(ui_font_bold) UI_Label(job->job_number);
        if(should_include_name)
        {
            UI_TextTruncSuffix(S8("...")) UI_Label(job->job_name);
        }
    }
    UI_Use use = UI_UseFromNode(employee_emblem);
    if(use.is_dragging && 0 == ptbd_drag_drop.count)
    {
        PTBD_DragRow(DB_Table_Jobs, *job);
    }
    return use;
}

Function UI_Use
PTBD_BuildInvoiceEmblem(DB_Row *invoice)
{
    if(0 == invoice)
    {
        UI_SetNextStrokeWidth(0.0f);
        UI_SetNextBgCol(ptbd_grey);
    }
    else if(DB_InvoiceNumber_Draft == invoice->invoice_number)
    {
        UI_SetNextTexture(ptbd_gradient_vertical);
        UI_SetNextTextureRegion(Range2F(V2F(0.0f, 0.95f), V2F(1.0f, 0.15f)));
        UI_SetNextBgCol(PTBD_ColourFromInvoiceId(invoice->invoice_id));
        UI_SetNextStrokeWidth(0.0f);
    }
    else if(DB_InvoiceNumber_WrittenOff == invoice->invoice_number)
    {
        UI_SetNextStrokeWidth(4.0f);
        UI_SetNextFgCol(ColFromHex(0xbf616aff));
        UI_SetNextBgCol(U4F(0.0f));
    }
    else
    {
        UI_SetNextStrokeWidth(0.0f);
        UI_SetNextBgCol(PTBD_ColourFromInvoiceId(invoice->id));
    }
    
    UI_Node *invoice_emblem =
        UI_NodeMakeFromFmt(UI_Flags_DrawFill |
                           UI_Flags_DrawStroke |
                           UI_Flags_DrawDropShadow |
                           UI_Flags_DrawHotStyle |
                           UI_Flags_DrawActiveStyle |
                           UI_Flags_AnimateHot |
                           UI_Flags_AnimateActive |
                           UI_Flags_Pressable,
                           "invoice emblem %lld",
                           invoice == 0 ? 0 : invoice->id);
    UI_Use use = UI_UseFromNode(invoice_emblem);
    if(use.is_hovering)
    {
        PTBD_ScrollToRow(DB_Table_Invoices, invoice == 0 ? 0 : invoice->id);
    }
    if(use.is_dragging && 0 == ptbd_drag_drop.count && 0 != invoice)
    {
        PTBD_DragRow(DB_Table_Invoices, *invoice);
    }
    return use;
}

Function void
PTBD_WindowHookOpen(OpaqueHandle window)
{
    PTBD_Application *app = G_WindowUserDataGet(window);
    app->is_running = 1;
}

Function void
PTBD_ExcelFileFromInvoice(DB_Row *invoice, S8 filename)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    S8 filename_with_null_terminator = S8Clone(scratch.arena, filename);
    
    lxw_workbook *workbook = workbook_new(filename_with_null_terminator.buffer);
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, "PTBD Export");
    
    lxw_format *default_format = workbook_add_format(workbook);
    format_set_align(default_format, LXW_ALIGN_LEFT);
    format_set_align(default_format, LXW_ALIGN_VERTICAL_TOP);
    format_set_text_wrap(default_format);
    
    lxw_format *draft_format = workbook_add_format(workbook);
    format_set_align(draft_format, LXW_ALIGN_LEFT);
    format_set_align(draft_format, LXW_ALIGN_VERTICAL_TOP);
    format_set_font_color(draft_format, LXW_COLOR_RED);
    format_set_text_wrap(draft_format);
    
    lxw_format *heading_format = workbook_add_format(workbook);
    format_set_bold(heading_format);
    format_set_align(heading_format, LXW_ALIGN_CENTER);
    format_set_align(heading_format, LXW_ALIGN_VERTICAL_TOP);
    
    lxw_format *bold_format = workbook_add_format(workbook);
    format_set_bold(bold_format);
    format_set_align(bold_format, LXW_ALIGN_VERTICAL_TOP);
    
    lxw_format *date_format = workbook_add_format(workbook);
    format_set_num_format(date_format, "dd/mm/yyyy");
    format_set_align(date_format, LXW_ALIGN_LEFT);
    format_set_align(date_format, LXW_ALIGN_VERTICAL_TOP);
    
    lxw_format *invoice_for_table_heading_format = workbook_add_format(workbook);
    format_set_bold(invoice_for_table_heading_format);
    format_set_bottom(invoice_for_table_heading_format, LXW_BORDER_THIN);
    format_set_bottom_color(invoice_for_table_heading_format, LXW_COLOR_BLACK);
    format_set_align(invoice_for_table_heading_format, LXW_ALIGN_VERTICAL_TOP);
    
    lxw_format *total_due_format = workbook_add_format(workbook);
    format_set_bold(total_due_format);
    format_set_top(total_due_format, LXW_BORDER_THIN);
    format_set_top_color(total_due_format, LXW_COLOR_BLACK);
    format_set_num_format(total_due_format, "£#,##0.00;[Red]-£#,##0.00");
    format_set_align(total_due_format, LXW_ALIGN_VERTICAL_TOP);
    
    lxw_format *currency_format = workbook_add_format(workbook);
    format_set_num_format(currency_format, "£#,##0.00;[Red]-£#,##0.00");
    format_set_align(currency_format, LXW_ALIGN_VERTICAL_TOP);
    
    int padding = 8;
    
    S8 logo_path = FilenamePush(scratch.arena, OS_S8FromStdPath(scratch.arena, OS_StdPath_ExecutableDir), S8("logo.png"));
    if(!OS_FilePropertiesFromFilename(logo_path).flags & OS_FilePropertiesFlags_Exists)
    {
        OS_TCtxErrorPush(S8("If you want to export with a banner, make sure you have a file called 'logo.png' next to the exe."));
    }
    worksheet_merge_range(worksheet, 0, 0, 0, 2, 0, 0);
    worksheet_set_row(worksheet, 0, 49.5 + padding, 0);
    lxw_image_options header_image_options = { .x_scale = 0.23f, .y_scale = 0.23f };
    worksheet_insert_image_opt(worksheet, 0, 0, logo_path.buffer, &header_image_options);
    
    worksheet_merge_range(worksheet, 1, 0, 1, 2, "INVOICE", heading_format);
    worksheet_merge_range(worksheet, 2, 1, 2, 2, 0, 0);
    worksheet_merge_range(worksheet, 3, 1, 3, 2, 0, 0);
    worksheet_merge_range(worksheet, 4, 1, 4, 2, 0, 0);
    worksheet_merge_range(worksheet, 5, 1, 5, 2, 0, 0);
    worksheet_merge_range(worksheet, 6, 1, 6, 2, 0, 0);
    worksheet_set_column(worksheet, 0, 0, 11.29, default_format);
    worksheet_set_column(worksheet, 1, 1, 62.57, default_format);
    worksheet_set_row(worksheet, 5, 90, default_format);
    
    worksheet_write_string(worksheet, 3, 0, "Invoice no:", bold_format);
    worksheet_write_string(worksheet, 4, 0, "Date:", bold_format);
    worksheet_write_string(worksheet, 5, 0, "To:", bold_format);
    worksheet_write_string(worksheet, 6, 0, "Job:", bold_format);
    
    worksheet_set_row(worksheet, 3, 15 + padding, 0);
    worksheet_set_row(worksheet, 4, 15 + padding, 0);
    worksheet_set_row(worksheet, 6, 15 + padding, 0);
    
    DateTime date = DateTimeFromDenseTime(invoice->date);
    lxw_datetime xlsx_date =
    {
        .year = date.year,
        .month = date.mon + 1,
        .day = date.day + 1,
    };
    if(DB_InvoiceNumber_Draft == invoice->invoice_number)
    {
        worksheet_write_string(worksheet, 3, 1, "draft", draft_format);
    }
    else
    {
        worksheet_write_number(worksheet, 3, 1, invoice->invoice_number, default_format);
    }
    worksheet_write_datetime(worksheet, 4, 1, &xlsx_date, date_format);
    worksheet_write_string(worksheet, 5, 1, invoice->address.buffer, default_format);
    worksheet_write_string(worksheet, 6, 1, invoice->title.buffer, default_format);
    
    worksheet_merge_range(worksheet, 8, 0, 8, 2, "Invoice for:", invoice_for_table_heading_format);
    
    worksheet_set_row(worksheet, 9, padding, NULL);
    
    U64 current_row = 10;
    for(S8Split split = S8SplitMake(invoice->description, S8("\n"), 0); S8SplitNext(&split); current_row += 1)
    {
        worksheet_merge_range(worksheet, current_row, 0, current_row, 1, S8Clone(scratch.arena, split.current).buffer, default_format);
    }
    
    worksheet_set_row(worksheet, current_row, padding, NULL);
    current_row += 1;
    
    worksheet_merge_range(worksheet, current_row, 0, current_row, 1, "TOTAL DUE", total_due_format);
    worksheet_write_number(worksheet, current_row, 2, invoice->cost, total_due_format);
    
    worksheet_set_footer(worksheet,
                         "&LTERMS:\n"
                         " - Payment within 14 days of the date of this invoice, unless otherwise agreed.\n"
                         " - Cheques payable to PAUL THORNTON BUILDING DESIGN LTD.\n"
                         " - Bank Sort Code: 08-92-99, Bank Account Number: 69166600");
    
    lxw_error error = workbook_close(workbook);
    
    // TODO(tbt): display error
    
    ArenaTempEnd(scratch);
}

Function void
PTBD_ScrollToRow(DB_Table table, I64 id)
{
    OS_SemaphoreWait(ptbd_scroll_to_lock);
    ptbd_next_scroll_to[table] = id;
    OS_SemaphoreSignal(ptbd_scroll_to_lock);
}

Function void
PTBD_ApplyScrollToForList(DB_Table table, DB_Cache data, DB_Cache timesheets, PTBD_Index *index, PTBD_Selection *expanded, F32 *scroll, F32 window_size, F32 height_per_row, U64 records_per_row)
{
    if(0 == ptbd_scroll_to[table])
    {
        ptbd_did_scroll_to[table] = True;
        *scroll = 0;
    }
    else if(ptbd_scroll_to[table] > 0 || (0 != expanded && 0 != ptbd_scroll_to[DB_Table_Timesheets]))
    {
        F32 s = G_WindowScaleFactorGet(ui->window);
        
        B32 should_scroll = False;
        U64 row_index = 0;
        for(U64 record_index = 0; record_index < index->records_count; record_index += 1)
        {
            PTBD_IndexRecord *record = &index->records[record_index];
            DB_Row *row = &data.rows[record->key];
            if(row->id == ptbd_scroll_to[table])
            {
                should_scroll = True;
                ptbd_did_scroll_to[table] = True;
                break;
            }
            
            row_index += 1;
            
            if(PTBD_SelectionHas(expanded, row->id))
            {
                for(U64 timesheet_index = 0; timesheet_index < record->indices_count; timesheet_index += 1)
                {
                    DB_Row *timesheet = &timesheets.rows[record->indices[timesheet_index]];
                    if(timesheet->id == ptbd_scroll_to[DB_Table_Timesheets])
                    {
                        should_scroll = True;
                        ptbd_did_scroll_to[DB_Table_Timesheets] = True;
                        goto break_all;
                    }
                    
                    row_index += 1;
                }
            }
        }
        break_all:;
        
        if(should_scroll)
        {
            F32 new_view_offset = 0.5f*window_size - (0.5f + row_index / records_per_row)*height_per_row*s;
            if(Abs1F(*scroll - new_view_offset) > 0.5f*window_size)
            {
                *scroll = new_view_offset;
            }
        }
    }
}

Function void
PTBD_ApplyScrollToForHistogram(DB_Cache timesheets, PTBD_Index *index, F32 *scroll, F32 window_size, F32 height_per_row)
{
    if(0 == ptbd_scroll_to[DB_Table_Timesheets])
    {
        ptbd_did_scroll_to[DB_Table_Timesheets] = True;
        *scroll = 0;
    }
    else if(ptbd_scroll_to[DB_Table_Timesheets] > 0)
    {
        F32 s = G_WindowScaleFactorGet(ui->window);
        
        for(U64 row_index = 0; row_index < index->records_count; row_index += 1)
        {
            PTBD_IndexRecord *record = &index->records[row_index];
            for(U64 timesheet_index = 0; timesheet_index < record->indices_count; timesheet_index += 1)
            {
                DB_Row *timesheet = &timesheets.rows[record->indices[timesheet_index]];
                if(timesheet->id == ptbd_scroll_to[DB_Table_Timesheets])
                {
                    F32 new_view_offset = 0.5f*window_size - (0.5f + row_index)*height_per_row*s;
                    if(Abs1F(*scroll - new_view_offset) > 0.5f*window_size)
                    {
                        *scroll = new_view_offset;
                    }
                    
                    ptbd_did_scroll_to[DB_Table_Timesheets] = True;
                    goto break_all;
                }
            }
        }
        break_all:;
    }
}

Function V4F
PTBD_ColourFromInvoiceId(I64 invoice_id)
{
    I32 intermediates = 8;
    V4F colours[] =
    {
        ColFromHex(0xebcb8bff),
        ColFromHex(0xa3be8cff),
        ColFromHex(0x8fbcbbff),
        ColFromHex(0x88c0d0ff),
        ColFromHex(0x81a1c1ff),
        ColFromHex(0x5e81acff),
        ColFromHex(0xb48eadff),
        ColFromHex(0xd08770ff),
    };
    U64 hash[2]; MurmurHash3_x64_128(&invoice_id, sizeof(invoice_id), 0, hash);
    I32 intermediate_index = hash[0] % (intermediates*ArrayCount(colours));
    F32 fractional_index = (F32)intermediate_index / intermediates;
    I32 low_index = (I32)Floor1F(fractional_index) % ArrayCount(colours);
    I32 high_index = (I32)Ceil1F(fractional_index) % ArrayCount(colours);
    V4F result = ColMix(colours[low_index], colours[high_index], Fract1F(fractional_index));
    if(ptbd_is_dark_mode_t > 0.5f)
    {
        result = ColMix(result, ColFromHex(0x2e3440ff), 0.5f*(ptbd_is_dark_mode_t - 0.5f));
    }
    return result;
}

Function V4F
PTBD_ColourFromEmployeeId(I64 employee_id)
{
    ReadOnlyVar Persist V4F colours[] =
    {
        ColFromHexInitialiser(0x81a1c1ff),
        ColFromHexInitialiser(0x5e81acff),
        ColFromHexInitialiser(0xa3be8cff),
        ColFromHexInitialiser(0xd08770ff),
        ColFromHexInitialiser(0xb48eadff),
    };
    
    V4F result = ptbd_grey;
    if(0 != employee_id)
    {
        result = colours[employee_id % ArrayCount(colours)];
        if(ptbd_is_dark_mode_t > 0.5f)
        {
            result = ColMix(result, ColFromHex(0x2e3440ff), 0.5f*(ptbd_is_dark_mode_t - 0.5f));
        }
    }
    
    return result;
}

Function UI_Window
PTBD_UIWindowFromContainerScrollHeightPerRowAndRowsCount(UI_Node *container, F32 height_per_row, U64 rows_count, F32 vertical_scroll)
{
    UI_Window window = {0};
    
    F32 s = G_WindowScaleFactorGet(ui->window);
    
    window.pixel_range = Range1F(-vertical_scroll, container->computed_size.y - vertical_scroll);
    window.index_range = Range1U64(window.pixel_range.min / (height_per_row*s), window.pixel_range.max / (height_per_row*s) + 2);
    
    window.index_range.min = Min1U64(window.index_range.min, rows_count);
    window.index_range.max = Min1U64(window.index_range.max, rows_count);
    
    window.space_before = window.index_range.min*height_per_row;
    window.space_after = (rows_count - window.index_range.max)*height_per_row;
    
    return window;
}

Function UI_Node *
PTBD_Window(S8 title, PTBD_Zoom id, PTBD_Zoom *zoom)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    UI_Node *result = 0;
    
    UI_SetNextCornerRadius(16.0f);
    UI_SetNextChildrenLayout(Axis2_Y);
    UI_SetNextFgCol(ptbd_grey);
    UI_Node *window = UI_NodeMake(UI_Flags_DrawFill | UI_Flags_DrawStroke | UI_Flags_DrawDropShadow, S8(""));
    
    UI_Parent(window) UI_Width(UI_SizeFill()) UI_Height(UI_SizeFill())
    {
        UI_Spacer(UI_SizePx(8.0f, 1.0f));
        
        UI_Width(UI_SizeFromText(16.0f, 1.0f)) UI_Height(UI_SizeFromText(8.0f, 1.0f)) UI_Font(ui_font_bold) UI_FontSize(24)
        {
            UI_SetNextHotFgCol(ColFromHex(0x81a1c1ff));
            UI_SetNextActiveFgCol(ColFromHex(0x88c0d0ff));
            UI_Node *label = UI_NodeMakeFromFmt(UI_Flags_DrawText |
                                                UI_Flags_DrawHotStyle |
                                                UI_Flags_DrawActiveStyle |
                                                UI_Flags_Pressable |
                                                UI_Flags_NavDefaultFilter |
                                                UI_Flags_AnimateHot |
                                                UI_Flags_AnimateActive,
                                                "%.*s##window title",
                                                FmtS8(title));
            UI_Use use = UI_UseFromNode(label);
            if(use.is_double_clicked)
            {
                *zoom = id;
            }
        }
        
        UI_Row()
        {
            UI_Spacer(UI_SizePx(8.0f, 1.0f));
            result = UI_NodeMake(UI_Flags_DoNotMask, S8(""));
            UI_Spacer(UI_SizePx(8.0f, 1.0f));
        }
        UI_Spacer(UI_SizePx(8.0f, 1.0f));
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function void
PTBD_WindowHookDraw(OpaqueHandle window)
{
    PTBD_Application *app = G_WindowUserDataGet(window);
    
    Arena *frame_arena = G_WindowFrameArenaGet(window);
    Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
    
    UI_Begin(app->ui_state);
    
    OS_SemaphoreWait(ptbd_scroll_to_lock);
    MemoryCopy(ptbd_scroll_to, ptbd_next_scroll_to, sizeof(ptbd_scroll_to));
    for(DB_Table table_index = 0; table_index < DB_Table_MAX; table_index += 1)
    {
        if(ptbd_did_scroll_to[table_index])
        {
            ptbd_next_scroll_to[table_index] = -1;
        }
    }
    OS_SemaphoreSignal(ptbd_scroll_to_lock);
    
    UI_Animate1F(&ptbd_is_dark_mode_t, G_IsDarkMode(), 5.0f);
    
    G_WindowClearColourSet(window, ColMix(ColFromHex(0xeceff4ff), ColFromHex(0x2e3440ff), ptbd_is_dark_mode_t));
    ptbd_grey = ColMix(ColFromHex(0xd8dee9ff), ColFromHex(0x3b4252ff), ptbd_is_dark_mode_t);
    UI_FgColPush(ColMix(ColFromHex(0x2e3440ff), ColFromHex(0xd8dee9ff), ptbd_is_dark_mode_t));
    UI_BgColPush(ColMix(ColFromHex(0xeceff4ff), ColFromHex(0x2e3440ff), ptbd_is_dark_mode_t));
    
    UI_Animate1F(&app->is_loading_t, app->is_next_state_dirty, 15.0f);
    
    if(app->is_loading_t > 0.5f)
    {
        ui->default_cursor_kind = G_CursorKind_Loading;
    }
    else
    {
        ui->default_cursor_kind = G_CursorKind_Default;
    }
    
    if(app->should_copy_state)
    {
        OS_SemaphoreWait(app->state_lock);
        ArenaClear(app->state_arena);
        PTBD_CopyCriticalState(app->state_arena, &app->state, &app->next_state);
        M2C_DidCopyState(&app->m2c);
        OS_SemaphoreSignal(app->state_lock);
        
        PTBD_DraggingClear();
    }
    
    if(!app->is_next_state_dirty)
    {
        while(app->c2m.read_pos < app->c2m.write_pos)
        {
            PTBD_MsgQueue *queue = &app->c2m;
            OS_SemaphoreWait(queue->lock);
            {
                C2M_Kind kind;
                queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &kind, sizeof(kind));
                
                if(C2M_Kind_WroteNewRecord == kind)
                {
                    DB_Table table;
                    queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &table, sizeof(table));
                    I64 id;
                    queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &id, sizeof(id));
                    
                    if(DB_Table_Timesheets == table)
                    {
                        PTBD_SelectionSet(permanent_arena, app->selected_timesheets, id, True);
                    }
                }
            }
            OS_SemaphoreSignal(queue->lock);
        }
    }
    
    F32 padding = 8.0f;
    
    F32 s = G_WindowScaleFactorGet(ui->window);
    
    PTBD_DraggingBuildUI();
    
    UI_Column()
    {
        UI_Spacer(UI_SizePx(padding, 1.0f));
        
        UI_Height(UI_SizePx(40.0f, 1.0f)) UI_Row()
        {
            UI_Pad(UI_SizeFill()) UI_Width(UI_SizePct(0.3f, 1.0f))
            {
                UI_Width(UI_SizeFromText(8.0f, 1.0f)) UI_LabelFromIcon(FONT_Icon_Search);
                
                UI_Spacer(UI_SizePx(padding, 1.0f));
                
                UI_SetNextFgCol(ColMix(ptbd_grey, ColFromHex(0x81a1c1ff), app->is_loading_t));
                UI_SetNextCornerRadius(18.0f);
                UI_Use use = UI_LineEdit(S8("search"), &app->search_bar);
                if(use.is_edited)
                {
                    M2C_SetSearch(&app->m2c, S8FromBuilder(app->search_bar));
                }
                else if(use.is_left_up && ptbd_drag_drop.count > 0)
                {
                    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
                    
                    DB_Row *row = ptbd_drag_drop.data;
                    
                    if(DB_Table_Invoices == ptbd_drag_drop.kind)
                    {
                        S8 new_search = {0};
                        if(DB_InvoiceNumber_Draft == row->invoice_number)
                        {
                            new_search = S8FromFmt(scratch.arena, "Draft invoice %lld", row->id);
                        }
                        else if(DB_InvoiceNumber_WrittenOff == row->invoice_number)
                        {
                            new_search = S8("Written off");
                        }
                        else
                        {
                            new_search = S8FromFmt(scratch.arena, "Invoice %lld", row->invoice_number);
                        }
                        M2C_SetSearch(&app->m2c, new_search);
                        app->search_bar.len = 0;
                        S8BuilderAppend(&app->search_bar, new_search);
                    }
                    else if(DB_Table_Jobs == ptbd_drag_drop.kind)
                    {
                        S8 new_search = S8FromFmt(scratch.arena, "job %.*s", FmtS8(row->job_number));
                        M2C_SetSearch(&app->m2c, new_search);
                        app->search_bar.len = 0;
                        S8BuilderAppend(&app->search_bar, new_search);
                    }
                    else if(DB_Table_Employees == ptbd_drag_drop.kind)
                    {
                        S8 new_search = S8FromFmt(scratch.arena, "employee %.*s", FmtS8(row->employee_name));
                        M2C_SetSearch(&app->m2c, new_search);
                        app->search_bar.len = 0;
                        S8BuilderAppend(&app->search_bar, new_search);
                    }
                    else
                    {
                        OS_TCtxErrorPush(S8("Drag an invoice, job or employee here to search for relevant timesheets."));
                    }
                    
                    ArenaTempEnd(scratch);
                }
            }
            
            if(PTBD_Zoom_Overview == app->zoom)
            {
                UI_SetNextDefaultFlags(UI_Flags_NavDefaultFilter);
            }
            UI_Use overview_button = UI_IconButton(S8("overview"), FONT_Icon_ResizeSmall);
            UI_Animate1F(&overview_button.node->user_t, (app->zoom != PTBD_Zoom_Overview), 15.0f);
            overview_button.node->size[Axis2_X] = UI_SizePx(48.0f*overview_button.node->user_t, 1.0f);
            if(overview_button.is_pressed)
            {
                app->zoom = PTBD_Zoom_Overview;
            }
        }
        
        UI_Spacer(UI_SizePx(padding, 1.0f));
        
        if(PTBD_Zoom_Overview == app->zoom)
        {
            UI_Row()
            {
                UI_Spacer(UI_SizePx(padding, 1.0f));
                
                UI_Parent(PTBD_Window(S8("Timeline"), PTBD_Zoom_Timeline, &app->zoom))
                {
                    if(app->state.timeline_histogram_index.records_count > 0)
                    {
                        PTBD_BuildTimelineHistogram(app->state.cache[DB_Table_Timesheets], app->state.cache[DB_Table_Jobs], app->state.cache[DB_Table_Employees], &app->state.timeline_histogram_index, app->selected_timesheets, &app->timeline_view, &app->m2c, &app->timesheet_inspectors);
                    }
                    else
                    {
                        PTBD_BuildEmptyScreen(PTBD_BuildEmptyScreenFlags_ButtonToClearSearch, S8("No timesheets to show."), &app->m2c);
                    }
                }
                
                UI_Spacer(UI_SizePx(padding, 1.0f));
                
                UI_Column()
                {
                    UI_Parent(PTBD_Window(S8("Jobs"), PTBD_Zoom_Jobs, &app->zoom))
                    {
                        if(app->state.jobs_list_index.records_count > 0)
                        {
                            PTBD_BuildJobsList(app->state.cache[DB_Table_Jobs],
                                               app->state.cache[DB_Table_Timesheets],
                                               app->state.cache[DB_Table_Invoices],
                                               &app->state.jobs_list_index,
                                               app->selected_timesheets,
                                               app->expanded_jobs,
                                               &app->job_last_selected_id,
                                               &app->job_keyboard_focus_id,
                                               &app->timesheet_inspectors,
                                               &app->job_editor,
                                               &app->m2c);
                        }
                        else
                        {
                            PTBD_BuildEmptyScreen(PTBD_BuildEmptyScreenFlags_ButtonToClearSearch, S8("No jobs to show."), &app->m2c);
                        }
                    }
                    
                    UI_Spacer(UI_SizePx(padding, 1.0f));
                    
                    UI_Parent(PTBD_Window(S8("Invoicing"), PTBD_Zoom_Invoicing, &app->zoom))
                    {
                        UI_Animate1F(&app->invoicing_view.leader_column_width, 150.0f, 15.0f);
                        if(app->state.invoicing_histogram_index.records_count > 0)
                        {
                            PTBD_BuildInvoicingHistogram(app->state.cache[DB_Table_Timesheets], app->state.cache[DB_Table_Jobs], app->state.cache[DB_Table_Employees], &app->state.invoicing_histogram_index, app->selected_timesheets, &app->invoicing_view, &app->m2c, &app->timesheet_inspectors);
                        }
                        else
                        {
                            PTBD_BuildEmptyScreen(0, S8("Nothing to invoice."), &app->m2c);
                        }
                    }
                }
                
                UI_Spacer(UI_SizePx(padding, 1.0f));
                
                UI_Width(UI_SizePct(0.2f, 1.0f)) UI_Column() UI_Width(UI_SizeFill())
                {
                    UI_Parent(PTBD_Window(S8("Invoices"), PTBD_Zoom_Invoices, &app->zoom))
                    {
                        if(app->state.invoices_list_index.records_count > 0)
                        {
                            PTBD_BuildInvoicesList(app->state.cache[DB_Table_Invoices],
                                                   app->state.cache[DB_Table_Timesheets],
                                                   &app->state.invoices_list_index,
                                                   &app->invoice_keyboard_focus_id,
                                                   &app->invoice_editor,
                                                   &app->m2c);
                        }
                        else
                        {
                            PTBD_BuildEmptyScreen(0, S8("No invoices to show."), &app->m2c);
                        }
                    }
                    
                    UI_Spacer(UI_SizePx(padding, 1.0f));
                    
                    UI_Height(UI_SizePct(0.2f, 1.0f)) UI_Parent(PTBD_Window(S8("Employees"), PTBD_Zoom_Employees, &app->zoom)) UI_Height(UI_SizeFill())
                    {
                        if(app->state.employees_list_index.records_count > 0)
                        {
                            PTBD_BuildEmployeesList(app->state.cache[DB_Table_Employees], &app->state.employees_list_index, &app->employee_keyboard_focus_id, &app->employee_editor, &app->m2c);
                        }
                        else
                        {
                            PTBD_BuildEmptyScreen(0, S8("No employees to show."), &app->m2c);
                        }
                    }
                }
                
                UI_Spacer(UI_SizePx(padding, 1.0f));
            }
        }
        else if(PTBD_Zoom_Timeline == app->zoom)
        {
            if(app->state.timeline_histogram_index.records_count > 0)
            {
                PTBD_BuildTimelineHistogram(app->state.cache[DB_Table_Timesheets],
                                            app->state.cache[DB_Table_Timesheets],
                                            app->state.cache[DB_Table_Employees],
                                            &app->state.timeline_histogram_index,
                                            app->selected_timesheets,
                                            &app->timeline_view,
                                            &app->m2c,
                                            &app->timesheet_inspectors);
            }
            else
            {
                PTBD_BuildEmptyScreen(PTBD_BuildEmptyScreenFlags_ButtonToClearSearch, S8("No timesheets to show."), &app->m2c);
            }
        }
        else if(PTBD_Zoom_Jobs == app->zoom)
        {
            if(app->state.jobs_list_index.records_count > 0)
            {
                PTBD_BuildJobsList(app->state.cache[DB_Table_Jobs],
                                   app->state.cache[DB_Table_Timesheets],
                                   app->state.cache[DB_Table_Invoices],
                                   &app->state.jobs_list_index,
                                   app->selected_timesheets,
                                   app->expanded_jobs,
                                   &app->job_last_selected_id,
                                   &app->job_keyboard_focus_id,
                                   &app->timesheet_inspectors,
                                   &app->job_editor,
                                   &app->m2c);
            }
            else
            {
                PTBD_BuildEmptyScreen(PTBD_BuildEmptyScreenFlags_ButtonToClearSearch, S8("No jobs to show."), &app->m2c);
            }
        }
        else if(PTBD_Zoom_Invoicing == app->zoom)
        {
            UI_Animate1F(&app->invoicing_view.leader_column_width, 300.0f, 15.0f);
            if(app->state.invoicing_histogram_index.records_count > 0)
            {
                PTBD_BuildInvoicingHistogram(app->state.cache[DB_Table_Timesheets],
                                             app->state.cache[DB_Table_Jobs], 
                                             app->state.cache[DB_Table_Employees],
                                             &app->state.invoicing_histogram_index,
                                             app->selected_timesheets,
                                             &app->invoicing_view,
                                             &app->m2c,
                                             &app->timesheet_inspectors);
            }
            else
            {
                PTBD_BuildEmptyScreen(0, S8("Nothing to invoice."), &app->m2c);
            }
        }
        else if(PTBD_Zoom_Invoices == app->zoom)
        {
            UI_Row()
            {
                UI_Parent(PTBD_Window(S8("Invoicing"), PTBD_Zoom_Invoicing, &app->zoom))
                {
                    UI_Animate1F(&app->invoicing_view.leader_column_width, 150.0f, 15.0f);
                    if(app->state.invoicing_histogram_index.records_count > 0)
                    {
                        PTBD_BuildInvoicingHistogram(app->state.cache[DB_Table_Timesheets],
                                                     app->state.cache[DB_Table_Jobs],
                                                     app->state.cache[DB_Table_Employees],
                                                     &app->state.invoicing_histogram_index,
                                                     app->selected_timesheets,
                                                     &app->invoicing_view,
                                                     &app->m2c,
                                                     &app->timesheet_inspectors);
                    }
                    else
                    {
                        PTBD_BuildEmptyScreen(0, S8("Nothing to invoice."), &app->m2c);
                    }
                }
                
                UI_Spacer(UI_SizePx(padding, 1.0f));
                
                UI_Width(UI_SizePct(0.2f, 1.0f)) UI_Parent(PTBD_Window(S8("Invoices"), PTBD_Zoom_Invoices, &app->zoom)) UI_Width(UI_SizeFill())
                {
                    if(app->state.invoices_list_index.records_count > 0)
                    {
                        PTBD_BuildInvoicesList(app->state.cache[DB_Table_Invoices],
                                               app->state.cache[DB_Table_Timesheets],
                                               &app->state.invoices_list_index,
                                               &app->invoice_keyboard_focus_id,
                                               &app->invoice_editor,
                                               &app->m2c);
                    }
                    else
                    {
                        PTBD_BuildEmptyScreen(0, S8("No invoices to show."), &app->m2c);
                    }
                }
            }
        }
        else if(PTBD_Zoom_Employees == app->zoom)
        {
            if(app->state.employees_list_index.records_count > 0)
            {
                PTBD_BuildEmployeesList(app->state.cache[DB_Table_Employees], &app->state.employees_list_index, &app->employee_keyboard_focus_id, &app->employee_editor, &app->m2c);
            }
            else
            {
                PTBD_BuildEmptyScreen(0, S8("No employees to show."), &app->m2c);
            }
        }
        
        UI_Spacer(UI_SizePx(padding, 1.0f));
        
        if(!app->invoicing_view.is_dragging_selection_rect &&
           !app->timeline_view.is_dragging_selection_rect)
        {
            for(PTBD_Selection *chunk = app->selected_timesheets; 0 != chunk; chunk = chunk->next)
            {
                for(U64 i = 0; i < chunk->cap; i += 1)
                {
                    PTBD_RequireInspectorForTimesheet(OS_TCtxGet()->permanent_arena, app->state.cache[DB_Table_Timesheets], &app->timesheet_inspectors, chunk->ids[i]);
                }
            }
        }
        PTBD_UpdateInspectors(&app->timesheet_inspectors, app->state.cache[DB_Table_Timesheets]);
        
        OS_TCtxErrorScopePush();
        if(0 != app->timesheet_inspectors.first)
        {
            PTBD_BuildInspectorsPanel(&app->timesheet_inspectors,
                                      app->state.cache[DB_Table_Timesheets],
                                      app->state.cache[DB_Table_Jobs],
                                      app->state.cache[DB_Table_Employees],
                                      app->state.cache[DB_Table_Invoices]);
        }
        ErrorScope inspectors_errors = OS_TCtxErrorScopePop(frame_arena);
        
        if(app->timesheet_inspectors.last_edit_time + 300000 < OS_TimeInMicroseconds())
        {
            B32 did_write = False;
            for(PTBD_Inspector *inspector = app->timesheet_inspectors.first; 0 != inspector; inspector = inspector->next)
            {
                if(inspector->should_write)
                {
                    PTBD_PushWriteRecordCmdFromInspector(&app->m2c, inspector, app->state.cache[DB_Table_Jobs], app->state.cache[DB_Table_Timesheets], app->state.cache[DB_Table_Employees]);
                    did_write = True;
                }
            }
            if(did_write)
            {
                M2C_Reload(&app->m2c);
            }
            
            if(!app->timesheet_inspectors_have_errors)
            {
                //OS_TCtxErrorScopeMerge(&inspectors_errors);
            }
            app->timesheet_inspectors_have_errors = (inspectors_errors.count > 0);
        }
        
        UI_Spacer(UI_SizePx(padding, 1.0f));
        
        U64 now = OS_TimeInMicroseconds();
        for(ErrorMsg *msg = OS_TCtxErrorFirst(); 0 != msg; msg = msg->next)
        {
            U64 display_time = 10000000ULL;
            U64 difference = now - msg->timestamp;
            U64 in = 250000ULL;
            U64 out = 500000ULL;
            if(difference < display_time)
            {
                F32 h = 1.0f;
                if(difference < in)
                {
                    F32 l = 1.0f - (F32)difference / in;
                    h = 1.0f - l*l;
                    G_DoNotWaitForEventsUntil(OS_TimeInMicroseconds() + display_time);
                }
                else if(difference > display_time - out)
                {
                    F32 l = 1.0f - (F32)(display_time - difference) / out;
                    h = 1.0f - l*l;
                }
                
                UI_SetNextHeight(UI_SizePx(32.0f*h, 1.0f));
                UI_SetNextChildrenLayout(Axis2_X);
                UI_Node *row = UI_NodeMakeFromFmt(0, "error msg %p", msg);
                UI_Parent(row) UI_FgCol(ColFromHex(0xbf616aff))
                {
                    UI_Font(ui_font_bold) UI_Label(msg->string);
                }
            }
        }
    }
    
    G_EventQueue *events = G_WindowEventQueueGet(window);
    if(G_EventQueueHasKeyDown(events, G_Key_Z, G_ModifierKeys_Ctrl, True))
    {
        PTBD_Undo(&app->m2c, &app->timesheet_inspectors);
    }
    if(G_EventQueueHasKeyDown(events, G_Key_Z, G_ModifierKeys_Ctrl | G_ModifierKeys_Shift, True))
    {
        PTBD_Redo(&app->m2c, &app->timesheet_inspectors);
    }
    if(G_EventQueueHasKeyDown(events, G_Key_Esc, 0, False))
    {
        UI_Key key = UI_NavInterfaceStackPop();
        if(UI_KeyIsNil(&key) && PTBD_Zoom_Overview != app->zoom)
        {
            app->zoom = PTBD_Zoom_Overview;
        }
    }
    if(G_EventQueueHasKeyDown(events, G_Key_Delete, G_ModifierKeys_Ctrl, True))
    {
        M2C_DeleteSelectedRows(&app->m2c, DB_Table_Timesheets, app->selected_timesheets);
        M2C_Reload(&app->m2c);
    }
    if(G_EventQueueHasKeyDown(events, G_Key_N, G_ModifierKeys_Ctrl, True))
    {
        DenseTime date_to_add_new_timesheet_to = DenseTimeFromDateTime(OS_NowLTC());
        DenseTime job_to_add_new_timesheet_to = 0;
        
        B32 is_first = True;
        B32 all_selected_timesheets_are_from_same_date = True;
        B32 all_selected_timesheets_are_from_same_job = True;
        DenseTime selected_timesheets_date = date_to_add_new_timesheet_to;
        I64 selected_timesheets_job = job_to_add_new_timesheet_to;
        for(PTBD_Selection *chunk = app->selected_timesheets; 0 != chunk && (all_selected_timesheets_are_from_same_date || all_selected_timesheets_are_from_same_job); chunk = chunk->next)
        {
            for(U64 id_index = 0; id_index < chunk->cap && (all_selected_timesheets_are_from_same_date || all_selected_timesheets_are_from_same_job); id_index += 1)
            {
                I64 timesheet_id = chunk->ids[id_index];
                DB_Row *timesheet = DB_RowFromId(app->state.cache[DB_Table_Timesheets], timesheet_id);
                if(0 != timesheet)
                {
                    if(is_first)
                    {
                        selected_timesheets_date = timesheet->date;
                        selected_timesheets_job = timesheet->job_id;
                        is_first = False;
                    }
                    else
                    {
                        if(selected_timesheets_date != timesheet->date)
                        {
                            all_selected_timesheets_are_from_same_date = False;
                        }
                        if(selected_timesheets_job != timesheet->job_id)
                        {
                            all_selected_timesheets_are_from_same_job = False;
                        }
                    }
                }
            }
        }
        if(all_selected_timesheets_are_from_same_date)
        {
            date_to_add_new_timesheet_to = selected_timesheets_date;
        }
        if(all_selected_timesheets_are_from_same_job)
        {
            job_to_add_new_timesheet_to = selected_timesheets_job;
        }
        
        M2C_WriteTimesheet(&app->m2c, 0, date_to_add_new_timesheet_to, 0, job_to_add_new_timesheet_to, 4.0f, 0.0f, -1.0f, S8("description"), 0);
        M2C_Reload(&app->m2c);
    }
    
    if(!G_WindowKeyStateGet(ui->window, G_Key_MouseButtonLeft))
    {
        PTBD_DraggingClear();
    }
    
    UI_FgColPop();
    UI_BgColPop();
    
    UI_End();
}

Function void
PTBD_WindowHookClose(OpaqueHandle window)
{
    PTBD_Application *app = G_WindowUserDataGet(window);
    OS_InterlockedExchange1I(&app->is_running, 0);
}

Function void
PTBD_CopyDBCache(Arena *arena, DB_Cache *dst, DB_Cache *src)
{
    dst->rows_count = src->rows_count;
    
    dst->rows = PushArray(arena, DB_Row, dst->rows_count);
    MemoryCopy(dst->rows, src->rows, sizeof(dst->rows[0])*dst->rows_count);
    
    for(U64 row_index = 0; row_index < dst->rows_count; row_index += 1)
    {
        dst->rows[row_index].job_number = S8Clone(arena, src->rows[row_index].job_number);
        dst->rows[row_index].job_name = S8Clone(arena, src->rows[row_index].job_name);
        dst->rows[row_index].employee_name = S8Clone(arena, src->rows[row_index].employee_name);
        dst->rows[row_index].description = S8Clone(arena, src->rows[row_index].description);
        dst->rows[row_index].address = S8Clone(arena, src->rows[row_index].address);
        dst->rows[row_index].title = S8Clone(arena, src->rows[row_index].title);
    }
}

Function void
PTBD_CopyIndex(Arena *arena, PTBD_Index *dst, PTBD_Index *src)
{
    dst->records_count = src->records_count;
    
    dst->records = PushArray(arena, DB_Row, dst->records_count);
    MemoryCopy(dst->records, src->records, sizeof(dst->records[0])*dst->records_count);
    
    for(U64 record_index = 0; record_index < dst->records_count; record_index += 1)
    {
        dst->records[record_index].indices = PushArray(arena, U64, src->records[record_index].indices_count);
        MemoryCopy(dst->records[record_index].indices, src->records[record_index].indices, src->records[record_index].indices_count*sizeof(src->records[record_index].indices[0]));
    }
}

Function void
PTBD_CopyCriticalState(Arena *arena, PTBD_CriticalState *dst, PTBD_CriticalState *src)
{
    for(DB_Table table_index = 0; table_index < DB_Table_MAX; table_index += 1)
    {
        PTBD_CopyDBCache(arena, &dst->cache[table_index], &src->cache[table_index]);
    }
    PTBD_CopyIndex(arena, &dst->timeline_histogram_index, &src->timeline_histogram_index);
    PTBD_CopyIndex(arena, &dst->invoicing_histogram_index, &src->invoicing_histogram_index);
    PTBD_CopyIndex(arena, &dst->jobs_list_index, &src->jobs_list_index);
    PTBD_CopyIndex(arena, &dst->invoices_list_index, &src->invoices_list_index);
    PTBD_CopyIndex(arena, &dst->employees_list_index, &src->employees_list_index);
    dst->search.buffer = dst->search_buffer;
    dst->search.len = src->search.len;
    MemoryCopy(dst->search_buffer, src->search.buffer, src->search.len);
}

Function S8
PTBD_DBConfigFilename(Arena *arena)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    S8 result = FilenamePush(arena, OS_S8FromStdPath(scratch.arena, OS_StdPath_Config), S8("ptbd_db_path.txt"));
    ArenaTempEnd(scratch);
    return result;
}

Function S8
PTBD_DBFilename(Arena *arena)
{
    S8 result = {0};
    
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    
    S8 config_filename = PTBD_DBConfigFilename(scratch.arena);
    if(OS_FilePropertiesFromFilename(config_filename).flags & OS_FilePropertiesFlags_Exists)
    {
        S8 db_filename_from_file = OS_FileReadEntire(scratch.arena, config_filename);
        if(OS_FilePropertiesFromFilename(db_filename_from_file).flags & OS_FilePropertiesFlags_Exists)
        {
            result = S8Clone(arena, db_filename_from_file);
        }
    }
    
    if(0 == result.len)
    {
        S8 default_db_filename = FilenamePush(scratch.arena, OS_S8FromStdPath(scratch.arena, OS_StdPath_ExecutableDir), S8("ptbd.db"));
        if(OS_FilePropertiesFromFilename(default_db_filename).flags & OS_FilePropertiesFlags_Exists)
        {
            result = S8Clone(arena, default_db_filename);
        }
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function void
PTBD_Reload(Arena *arena, sqlite3 *db, DB_Cache caches[DB_Table_MAX])
{
    ArenaClear(arena);
    
    U64 t_j = 0;
    U64 t_t = 0;
    U64 t_e = 0;
    U64 t_i = 0;
    
    OS_InstrumentBlockCumulative(t_j) DB_GetJobs(arena, db, &caches[DB_Table_Jobs]);
    OS_InstrumentBlockCumulative(t_t) DB_GetTimesheets(arena, db, &caches[DB_Table_Timesheets]);
    OS_InstrumentBlockCumulative(t_e) DB_GetEmployees(arena, db, &caches[DB_Table_Employees]);
    OS_InstrumentBlockCumulative(t_i) DB_GetInvoices(arena, db, &caches[DB_Table_Invoices]);
}

Function void
PTBD_Refresh(Arena *arena, PTBD_CriticalState *state)
{
    ArenaClear(arena);
    
    state->timeline_histogram_index = PTBD_IndexMakeForTimelineHistogram(arena, state->cache[DB_Table_Timesheets], state->search);
    state->invoicing_histogram_index = PTBD_IndexMakeForInvoicingHistogram(arena, state->cache[DB_Table_Timesheets], state->cache[DB_Table_Jobs], state->search);
    state->jobs_list_index = PTBD_IndexMakeForBreakdownList(arena, state->cache[DB_Table_Timesheets], state->cache[DB_Table_Jobs], OffsetOf(DB_Row, job_id), state->search, True);
    state->invoices_list_index = PTBD_IndexMakeForBreakdownList(arena, state->cache[DB_Table_Timesheets], state->cache[DB_Table_Invoices], OffsetOf(DB_Row, invoice_id), state->search, True);
    state->employees_list_index = PTBD_IndexMakeForBreakdownList(arena, state->cache[DB_Table_Timesheets], state->cache[DB_Table_Employees], OffsetOf(DB_Row, employee_id), state->search, False);
}

Function B32
PTBD_SearchHasRow(S8 search, DB_Row *row)
{
    B32 result =
    (0 == search.len ||
     S8HasSubstring(row->job_number, search, MatchFlags_CaseInsensitive) ||
     S8HasSubstring(row->job_name, search, MatchFlags_CaseInsensitive) ||
     S8HasSubstring(row->employee_name, search, MatchFlags_CaseInsensitive) ||
     S8HasSubstring(row->description, search, MatchFlags_CaseInsensitive) ||
     S8HasSubstring(row->title, search, MatchFlags_CaseInsensitive) ||
     S8HasSubstring(row->address, search, MatchFlags_CaseInsensitive));
    
    if(S8Consume(&search, S8("invoice "), MatchFlags_CaseInsensitive))
    {
        U64 invoice_number = S8Parse1U64(search);
        if(invoice_number > 0)
        {
            result = (row->invoice_number == invoice_number);
        }
    }
    else if(S8Consume(&search, S8("draft invoice "), MatchFlags_CaseInsensitive))
    {
        U64 invoice_id = S8Parse1U64(search);
        if(invoice_id > 0)
        {
            result = (DB_InvoiceNumber_Draft == row->invoice_number && row->invoice_id == invoice_id);
        }
    }
    else if(S8Consume(&search, S8("job "), MatchFlags_CaseInsensitive))
    {
        result = S8Match(row->job_number, search, 0);
    }
    else if(S8Match(search, S8("written off"), MatchFlags_CaseInsensitive))
    {
        result = (DB_InvoiceNumber_WrittenOff == row->invoice_number);
    }
    else if(S8Match(search, S8("draft invoice"), MatchFlags_CaseInsensitive))
    {
        result = (DB_InvoiceNumber_Draft == row->invoice_number && row->invoice_id > 0);
    }
    else if(S8Consume(&search, S8("employee "), MatchFlags_CaseInsensitive))
    {
        result = S8Match(row->employee_name, search, 0);
    }
    
    return result;
}

Function PTBD_IndexCoord
PTBD_IndexCoordFromDBId(PTBD_Index *index, DB_Cache data, I64 id)
{
    PTBD_IndexCoord result = {0};
    
    for(U64 record_index = 0; record_index < index->records_count; record_index += 1)
    {
        PTBD_IndexRecord *record = &index->records[record_index];
        
        for(U64 i = 0; i < record->indices_count; i += 1)
        {
            DB_Row *row = &data.rows[record->indices[i]];
            if(row->id == id)
            {
                result.record = record_index;
                result.index = i;
                goto break_all;
            }
        }
    }
    break_all:;
    
    return result;
}
