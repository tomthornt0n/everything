
typedef struct PTBD_InvoiceEditor PTBD_InvoiceEditor;
struct PTBD_InvoiceEditor
{
    I64 id;
    S8Builder title;
    S8Builder address;
    S8Builder description;
    S8Builder cost;
};

Function void PTBD_InvoicesListNav     (void *user_data, UI_Node *node);
Function U64  PTBD_InvoicesListNavNext (PTBD_Index *index, DB_Cache invoices, I64 *keyboard_focus_id);
Function U64  PTBD_InvoicesListNavPrev (PTBD_Index *index, DB_Cache invoices, I64 *keyboard_focus_id);

Function void PTBD_BuildInvoicesList (DB_Cache invoices, DB_Cache timesheets, PTBD_Index *index, I64 *keyboard_focus_id, PTBD_InvoiceEditor *invoice_editor, PTBD_MsgQueue *m2c);
