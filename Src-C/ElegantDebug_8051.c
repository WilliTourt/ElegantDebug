/*******************************************************************************
 * @file    ElegantDebug_8051.c
 * @version 1.0
 * @brief   ANSI-colored debug logging for 8051/8052 (Keil C51), implementation.
 *
 * See ElegantDebug_8051.h for the rationale (no variadic functions, no
 * platform #if blocks, C90 only). Output format matches the original library:
 *
 *   [00:00:01.250] \033[94m\033[1m[INFO]\033[0m hello
 *
 * @author:    <your name>
 * @date:      2026-11-15
 ******************************************************************************/
#include "ElegantDebug_8051.h"

/*** State ********************************************************************/

volatile ed8051_u32 ed8051_tick_ms = 0UL;

static ed8051_bool _timestamp_enabled = ED8051_TRUE;
static ed8051_bool _color_enabled     = ED8051_TRUE;
static ed8051_bool _fileline_enabled  = ED8051_FALSE;

/*** Low level output *********************************************************/

/* Polled transmitter for UART mode 1.
 * NOTE: if your firmware serves TI inside a UART interrupt, TI will already be
 * cleared by the ISR and this loop would never finish. In that case define
 * ED8051_PUTC(c) to your own queue-based routine (see the header). */
void ed8051_putc_polling(unsigned char c)
{
    TI = 0;
    SBUF = c;
    while (!TI) { }
    TI = 0;
}

static void _putStr(const char *s)
{
    while (*s != '\0')
    {
        ED8051_PUTC(*s);
        s++;
    }
}

static void _put2(ed8051_u8 v)
{
    ED8051_PUTC((char)('0' + (v / 10U) % 10U));
    ED8051_PUTC((char)('0' + v % 10U));
}

static void _put3(ed8051_u16 v)
{
    ED8051_PUTC((char)('0' + (v / 100U) % 10U));
    ED8051_PUTC((char)('0' + (v / 10U) % 10U));
    ED8051_PUTC((char)('0' + v % 10U));
}

static void _putU32(ed8051_u32 v)
{
    ed8051_u8 buf[10];
    ed8051_u8 i = 0;

    if (v == 0UL)
    {
        ED8051_PUTC('0');
        return;
    }

    while (v > 0UL && i < 10U)
    {
        buf[i] = (ed8051_u8)(v % 10UL);
        v = v / 10UL;
        i++;
    }

    while (i > 0U)
    {
        i--;
        ED8051_PUTC((char)('0' + buf[i]));
    }
}

/* "[hh:mm:ss.mmm] " - same layout as the original _send() prefix. */
static void _putTimestamp(void)
{
    ed8051_u32 ms;
    ed8051_u32 s;

    ms = ed8051_tick_ms;
    s  = ms / 1000UL;

    ED8051_PUTC('[');
    _put2((ed8051_u8)((s / 3600UL) % 100UL));
    ED8051_PUTC(':');
    _put2((ed8051_u8)((s / 60UL) % 60UL));
    ED8051_PUTC(':');
    _put2((ed8051_u8)(s % 60UL));
    ED8051_PUTC('.');
    _put3((ed8051_u16)(ms % 1000UL));
    ED8051_PUTC(']');
    ED8051_PUTC(' ');
}

/* Timestamp (optional) + type prefix (colored or plain). */
static void _begin(const char *colored_prefix, const char *plain_prefix)
{
    if (_timestamp_enabled)
    {
        _putTimestamp();
    }
    _putStr(_color_enabled ? colored_prefix : plain_prefix);
}

/*** API **********************************************************************/

void debug_8051_init(ed8051_bool timestamp, ed8051_bool color,
                     ed8051_bool fileline)
{
    _timestamp_enabled = timestamp;
    _color_enabled     = color;
    _fileline_enabled  = fileline;
}

void debug_8051_tick(void)
{
    ed8051_tick_ms++;
}

void debug_8051_log(const char *text)
{
    if (text == (const char *)0)
    {
        return;
    }
    if (_timestamp_enabled)
    {
        _putTimestamp();
    }
    _putStr(text);
}

void debug_8051_info(const char *text)
{
    _begin(ED8051_INFO_TYPE, ED8051_INFO_TYPE_PLAIN);
    _putStr(text);
}

void debug_8051_ok(const char *text)
{
    _begin(ED8051_OK_TYPE, ED8051_OK_TYPE_PLAIN);
    _putStr(text);
}

void debug_8051_success(const char *text)
{
    _begin(ED8051_SUCCESS_TYPE, ED8051_SUCCESS_TYPE_PLAIN);
    _putStr(text);
}

void debug_8051_warning(const char *file, ed8051_u16 line, const char *text)
{
    _begin(ED8051_WARNING_TYPE, ED8051_WARNING_TYPE_PLAIN);

    if (_fileline_enabled && file != (const char *)0)
    {
        ED8051_PUTC('[');
        _putStr(file);
        ED8051_PUTC(':');
        _putU32((ed8051_u32)line);
        ED8051_PUTC(']');
        ED8051_PUTC(' ');
    }
    _putStr(text);
}

void debug_8051_error(const char *file, ed8051_u16 line, const char *text)
{
    _begin(ED8051_ERROR_TYPE, ED8051_ERROR_TYPE_PLAIN);

    if (_fileline_enabled && file != (const char *)0)
    {
        ED8051_PUTC('[');
        _putStr(file);
        ED8051_PUTC(':');
        _putU32((ed8051_u32)line);
        ED8051_PUTC(']');
        ED8051_PUTC(' ');
    }
    _putStr(text);
}

void debug_8051_log_u32(const char *label, ed8051_u32 value)
{
    if (_timestamp_enabled)
    {
        _putTimestamp();
    }
    if (label != (const char *)0)
    {
        _putStr(label);
    }
    _putU32(value);
    ED8051_PUTC('\r');
    ED8051_PUTC('\n');
}

void debug_8051_setTimestamp(ed8051_bool enabled)
{
    _timestamp_enabled = enabled;
}

void debug_8051_setColor(ed8051_bool enabled)
{
    _color_enabled = enabled;
}
