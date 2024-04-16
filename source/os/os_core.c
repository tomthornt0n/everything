
//~NOTE(tbt): console

Function void
OS_ConsoleOutputFmtV(char *fmt, va_list args)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S8 string = S8FromFmtV(scratch.arena, fmt, args);
    OS_ConsoleOutputS8(string);
    ArenaTempEnd(scratch);
}

Function void
OS_ConsoleOutputFmt(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    OS_ConsoleOutputFmtV(fmt, args);
    va_end(args);
}

Function void
OS_ConsoleOutputLine(S8 string)
{
    OS_ConsoleOutputS8(string);
    OS_ConsoleOutputS8(S8("\n"));
}

//~NOTE(tbt): time

Function F64
OS_TimeInSeconds(void)
{
    U64 microseconds = OS_TimeInMicroseconds();
    F64 seconds = (F64)microseconds * 0.000001;
    return seconds;
}
