
//~NOTE(tbt): key codes

Function S8
G_S8FromKey(G_Key key)
{
    S8 result;
    
    Persist S8 strings[G_Key_MAX + 1] =
    {
#define G_KeyDef(NAME, STRING, IS_REAL) S8Initialiser(STRING),
#include "graphics_keylist.h"
    };
    
    if(G_Key_None <= key && key <= G_Key_MAX)
    {
        result = strings[key];
    }
    else
    {
        result = S8("ERROR - invalid key");
    }
    
    return result;
}

Function G_ModifierKeys
G_ModifierFromKey(G_Key key)
{
    G_ModifierKeys result = 0;
    if(G_Key_Ctrl == key)
    {
        result = G_ModifierKeys_Ctrl;
    }
    else if(G_Key_Shift == key)
    {
        result = G_ModifierKeys_Shift;
    }
    else if(G_Key_Alt == key)
    {
        result = G_ModifierKeys_Alt;
    }
    else if(G_Key_Super == key)
    {
        result = G_ModifierKeys_Super;
    }
    return result;
}

//~NOTE(tbt): event queues

Function void
G_EventConsume(G_Event *event)
{
    event->is_consumed = True;
}


Function void
G_EventQueueClear(G_EventQueue *queue)
{
    queue->events_count = 0;
}

Function void
G_EventQueuePush(G_EventQueue *queue, G_Event event)
{
    if(queue->events_count < G_EventQueueCap)
    {
        queue->events[queue->events_count] = event;
        queue->events_count += 1;
    }
}

Function G_Event *
G_EventQueueIncrementNotConsumed(G_EventQueue *queue, G_Event *current)
{
    G_Event *result = current;
    for(;;)
    {
        if((U64)(result - queue->events) >= queue->events_count)
        {
            result = 0;
            break;
        }
        if(!result->is_consumed)
        {
            break;
        }
        result += 1;
    }
    return result;
}

Function G_Event *
G_EventQueueFirstGet(G_EventQueue *queue)
{
    G_Event *result = 0;
    if(0 != queue)
    {
        result = queue->events;
        result = G_EventQueueIncrementNotConsumed(queue, result);
    }
    return result;
}

Function G_Event *
G_EventQueueNextGet(G_EventQueue *queue, G_Event *current)
{
    G_Event *result = 0;
    if(0 != queue)
    {
        result = G_EventQueueIncrementNotConsumed(queue, current + 1);
    }
    return result;
}

//~NOTE(tbt): event loop wrappers

Function B32
G_EventQueueHasKeyEvent(G_EventQueue *queue,
                        G_Key key,
                        G_ModifierKeys modifiers,
                        B32 is_down,
                        B32 is_repeat,
                        B32 should_consume)
{
    B32 result = False;
    for(G_EventQueueForEach(queue, e))
    {
        if(G_EventKind_Key == e->kind &&
           key == e->key &&
           (G_ModifierKeys_Any == modifiers || modifiers == e->modifiers) &&
           is_down == e->is_down &&
           is_repeat == e->is_repeat)
        {
            result = True;
            if(should_consume)
            {
                G_EventConsume(e);
            }
            G_DoNotWaitForEventsUntil(OS_TimeInMicroseconds() + 500000ULL);
            break;
        }
    }
    return result;
}

Function B32
G_EventQueueHasKeyDown(G_EventQueue *queue,
                       G_Key key,
                       G_ModifierKeys modifiers,
                       B32 should_consume)
{
    B32 result = G_EventQueueHasKeyEvent(queue, key, modifiers, True, False, should_consume);
    return result;
}

Function B32
G_EventQueueHasKeyUp(G_EventQueue *queue,
                     G_Key key,
                     G_ModifierKeys modifiers,
                     B32 should_consume)
{
    B32 result = G_EventQueueHasKeyEvent(queue, key, modifiers, False, False, should_consume);
    return result;
}

Function V2F
G_EventQueueSumDelta(G_EventQueue *queue, G_EventKind filter, B32 should_consume)
{
    V2F delta = U2F(0.0f);
    for(G_EventQueueForEach(queue, e))
    {
        if(filter == e->kind)
        {
            delta = Add2F(delta, e->delta);
            if(should_consume)
            {
                G_EventConsume(e);
            }
        }
    }
    return delta;
}

Function G_Event *
G_EventQueueFirstEventFromKind(G_EventQueue *queue, G_EventKind kind, B32 should_consume)
{
    G_Event *result = 0;
    for(G_EventQueueForEach(queue, e))
    {
        if(kind == e->kind)
        {
            result = e;
            if(should_consume)
            {
                G_EventConsume(e);
            }
            break;
        }
    }
    return result;
}

//~NOTE(tbt): platform specific back-ends

#if BuildOSWindows
# include "graphics_w32.c"
#elif BuildOSMac
# include "graphics_mac.m"
#else
# error "no graphics backend for current platform"
#endif
