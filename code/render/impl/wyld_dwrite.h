/* date = May 17th 2024 9:33 pm */

#ifndef WYLD_DWRITE_H
#define WYLD_DWRITE_H

typedef struct {
    struct ID2D1RenderTarget *render_target;
    struct IDWriteFactory5 *write_factory;
    struct IDWriteTextFormat *text_format;
    struct ID2D1SolidColorBrush *colour_brush;
} DWrite_State;

typedef struct {
    int temp;
} GlyphInfo;


void dwrite_initialize(DWrite_State *dwrite_state, IDXGISurface *surface);
void dwrite_draw(DWrite_State *dwrite_state, WCHAR *wchar, u32 string_length);

#endif //WYLD_DWRITE_H