#include <stdio.h>

/* --- platform helper functions ---------------------------------------------*/

#if (defined (WIN32) || defined (WIN64))

#include <Windows.h>

void cursor_home (void)
{
   COORD c = {0, 0};
   SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

void clear_screen (void)
{
   CONSOLE_SCREEN_BUFFER_INFO ci;
   COORD c = {0, 0};
   DWORD written;
   HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
   WORD a = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

   cursor_home();
   SetConsoleTextAttribute(h, a);
   GetConsoleScreenBufferInfo(h, &ci);
   // fill attributes
   FillConsoleOutputAttribute(h, a, ci.dwSize.X * ci.dwSize.Y, c, &written);
   // fill characters
   FillConsoleOutputCharacter(h, ' ', ci.dwSize.X * ci.dwSize.Y, c, &written);
}

int get_term_size (int* w, int* h)
{
   CONSOLE_SCREEN_BUFFER_INFO ci;
   GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci);
   if (w)
      *w = ci.dwSize.X;
   if (h)
      *h = ci.dwSize.Y;
   return 0;
}

void set_color_red (void)
{
   SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                           FOREGROUND_RED | FOREGROUND_INTENSITY);
}

void set_color_green (void)
{
   SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                           FOREGROUND_GREEN | FOREGROUND_INTENSITY);
}

void set_color_blue (void)
{
   SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                           FOREGROUND_BLUE | FOREGROUND_INTENSITY);
}

void set_color_default (void)
{
   SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                           FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

#if (!defined (WIN32) && !defined (WIN64))
int snprintf(char* str, size_t size, const char* format, ...)
{
   va_list args;
   int n;

   // verify buffer size
   if (size <= 0)
       // empty buffer
      return -1;

  // use vsnprintf function
   va_start(args, format);
   n = _vsnprintf(str, size, format, args);
   va_end(args);

   // check for truncated text
   if (n == -1 || n >= (int)size)
   {   // text truncated
      str[size - 1] = 0;
      return -1;
   }
   // return number of characters written to the buffer
   return n;
}

#endif

#elif defined (POSIX)

void cursor_home (void)
{
   printf("\033" "[H");
}

void clear_screen (void)
{
   printf("\033" "[H" "\033" "[2J");
}

int get_term_size (int* w, int* h)
{
   struct winsize ws;
   int ret = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
   if (ret)
      return ret;
   if (ws.ws_col == 0)
      ws.ws_col = 120;
   if (ws.ws_row == 0)
      ws.ws_row = 40;
   if (w)
      *w = ws.ws_col;
   if (h)
      *h = ws.ws_row;
   return ret;
}

void set_color_red (void)
{
   printf("\033" "[0;1;31m");
}

void set_color_green (void)
{
   printf("\033" "[0;1;32m");
}

void set_color_blue (void)
{
   printf("\033" "[0;1;34m");
}

void set_color_default (void)
{
   printf("\033" "[0m");
}

#elif defined (VXWORKS)

void cursor_home (void)
{
   printf("\033" "[H");
}

void clear_screen (void)
{
   printf("\033" "[H" "\033" "[2J");
}

int get_term_size (int* w, int* h)
{
    /* assume a terminal 100 cols, 60 rows fix */
   *w = 100;
   *h = 60;
   return 0;
}

void set_color_red (void)
{
   printf("\033" "[0;1;31m");
}

void set_color_green (void)
{
   printf("\033" "[0;1;32m");
}

void set_color_blue (void)
{
   printf("\033" "[0;1;34m");
}

void set_color_default (void)
{
   printf("\033" "[0m");
}

#else
#error "Target not defined!"
#endif
