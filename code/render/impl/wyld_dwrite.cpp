#include <windows.h>
#include <dwrite.h>
#include <dwrite_3.h>
#include <d2d1.h>
#include <d3d11.h>
#include <d3d11_1.h>

extern "C" {
#include "../../wyld_base.h"
#include "wyld_dwrite.h"
}

namespace {
    // https://graphics.stanford.edu/%7Eseander/bithacks.html#RoundUpPowerOf2
    inl u32 next_power_of_two_u32(u32 v) {
        u32 result = 1;
        if (v != 0) {
            --v;
            v |= v >> 1;
            v |= v >> 2;
            v |= v >> 4;
            v |= v >> 8;
            v |= v >> 16;
            ++v;
        }
        
        return(result);
    }
}

// When DWRITE does layout / text rendering, it needs to access actual font data. 
// That font data is stored in a font face object called IDWriteFontFace
// TODO(christian): I probably need to separate the font initialization from font resize
extern "C" void
dwrite_initialize(DWrite_State *dwrite_state, IDXGISurface *surface) {
    // https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwritefactory
    HRESULT hresult = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory5),
                                          reinterpret_cast<IUnknown **>(&dwrite_state->write_factory));
    
    assert_true(hresult == S_OK);
    
    // TODO(christian): https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwritefontcollection
    // https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwritefontcollection
    // https://learn.microsoft.com/en-us/windows/win32/directwrite/custom-font-sets-win10
    // apparently, you can specify your own font collection. I want to use my own.
    
    IDWriteFontSetBuilder1 *font_set_builder = nullptr;
    IDWriteFontFile *font_file = nullptr;
    IDWriteFontSet *font_set = nullptr;
    IDWriteFontCollection1 *font_collection = nullptr;
    
    IDWriteRenderingParams *default_rendering_params = nullptr;
    IDWriteRenderingParams *rendering_params = nullptr;
    IDWriteFontFace *font_face = nullptr;
    DWRITE_FONT_METRICS font_metrics;
    IDWriteGdiInterop *gdi_interop = nullptr;
    IDWriteBitmapRenderTarget *bitmap_render_target = nullptr;
    
    hresult = dwrite_state->write_factory->CreateFontSetBuilder(&font_set_builder);
    assert_true(hresult == S_OK);
    
    //GetFullPathNameA(""[in]  LPCSTR lpFileName, [in]  DWORD  nBufferLength, [out] LPSTR  lpBuffer, [out] LPSTR  *lpFilePart);
    hresult = dwrite_state->write_factory->CreateFontFileReference(L"..\\data\\assets\\fonts\\dos437.ttf", nullptr, &font_file);
    assert_true(hresult == S_OK);
    
    hresult = dwrite_state->write_factory->CreateFontFace(DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &font_file,
                                                          0, DWRITE_FONT_SIMULATIONS_NONE, &font_face);
    assert_true(hresult == S_OK);
    
    hresult = dwrite_state->write_factory->CreateRenderingParams(&default_rendering_params);
    assert_true(hresult == S_OK);
    
    hresult = dwrite_state->write_factory->CreateCustomRenderingParams(1.0f, default_rendering_params->GetEnhancedContrast(),
                                                                       default_rendering_params->GetClearTypeLevel(),
                                                                       default_rendering_params->GetPixelGeometry(),
                                                                       DWRITE_RENDERING_MODE_NATURAL,
                                                                       &rendering_params);
    assert_true(hresult == S_OK);
    
    font_face->GetMetrics(&font_metrics);
    
    // these we probably need to store in some glyph struct
    u32 glyph_count = font_face->GetGlyphCount();
    
    f32 dots_per_inch = 96.0f;
    f32 point_size = 8.0f;
    f32 points_to_inch = 1.0f / 72.0f;
    f32 pixel_per_em = point_size * points_to_inch * dots_per_inch;
    f32 ppem_per_design_unit = pixel_per_em / (f32)font_metrics.designUnitsPerEm;
    
    // https://learn.microsoft.com/en-us/windows/win32/directwrite/interoperating-with-gdi
    hresult = dwrite_state->write_factory->GetGdiInterop(&gdi_interop);
    assert_true(hresult == S_OK);
    
    u32 bitmap_width = (u32)((f32)font_metrics.capHeight * ppem_per_design_unit) * 8;
    u32 bitmap_height = bitmap_width;
    hresult = gdi_interop->CreateBitmapRenderTarget(nullptr, bitmap_width, bitmap_height, &bitmap_render_target);
    assert_true(hresult == S_OK);
    
    HDC device_ctx = bitmap_render_target->GetMemoryDC();
    
    // clear render targe
    // https://learn.microsoft.com/en-us/windows/win32/gdi/setting-the-pen-or-brush-color
    HGDIOBJ gdi_object = SelectObject(device_ctx, GetStockObject(DC_PEN));
    SetDCPenColor(device_ctx, RGB(0, 0, 0));
    SelectObject(device_ctx, GetStockObject(DC_BRUSH));
    SetDCBrushColor(device_ctx, RGB(0, 0, 0));
    Rectangle(device_ctx, 0, 0, bitmap_width, bitmap_height);
    SelectObject(device_ctx, gdi_object);
    
    u32 font_atlas_width = bitmap_width / 2;
    u32 font_atlas_height = bitmap_height / 2;
    
    if (font_atlas_width < 16) {
        font_atlas_width = 16;
    } else {
        font_atlas_width = next_power_of_two_u32(font_atlas_width);
    }
    
    if (font_atlas_height < 256) {
        font_atlas_height = 256;
    } else {
        font_atlas_height = next_power_of_two_u32(font_atlas_height);
    }
    
    u8 *atlas_memory = new u8[font_atlas_height * font_atlas_width * sizeof(u32)]{};
    
    
#if 1
    // TODO(christian): this is not compatible on Windows 10 versions earlier than the Windows 10 Creators Update.
    // See https://learn.microsoft.com/en-us/windows/win32/directwrite/custom-font-sets-win10 for compatibility
    hresult = font_set_builder->AddFontFile(font_file);
    assert_true(hresult == S_OK);
    
    hresult = font_set_builder->CreateFontSet(&font_set);
    assert_true(hresult == S_OK);
    
    hresult = dwrite_state->write_factory->CreateFontCollectionFromFontSet(font_set, &font_collection);
    assert_true(hresult == S_OK);
#endif
    
    hresult = dwrite_state->write_factory->CreateTextFormat(L"dos437", font_collection, DWRITE_FONT_WEIGHT_NORMAL,
                                                            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                                            8.0f, L"en-US", &(dwrite_state->text_format));
    assert_true(hresult == S_OK);
    
    ID2D1Factory *d2d_factory = nullptr;
    D2D1_FACTORY_OPTIONS d2d_factory_opts {
        D2D1_DEBUG_LEVEL_NONE
    };
    
    hresult = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory),
                                &d2d_factory_opts, reinterpret_cast<void **>(&d2d_factory));
    
    assert_true(hresult == S_OK);
    
    // https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nn-dxgi-idxgisurface
    
    D2D1_RENDER_TARGET_PROPERTIES surface_render_prop;
    surface_render_prop.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    surface_render_prop.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
    surface_render_prop.dpiX = 0;
    surface_render_prop.dpiY = 0;
    surface_render_prop.usage = D2D1_RENDER_TARGET_USAGE_NONE;
    surface_render_prop.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
    
    hresult = d2d_factory->CreateDxgiSurfaceRenderTarget(surface, surface_render_prop, &(dwrite_state->render_target));
    assert_true(hresult == S_OK);
    
    hresult = dwrite_state->render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 1.0f),
                                                                 &(dwrite_state->colour_brush));
    
    assert_true(hresult == S_OK);
}

// NOTE(christian): I plan on rasterizing fonts on the fly. Should I? NO.
extern "C" void
dwrite_draw(DWrite_State *dwrite_state, WCHAR *wchar, u32 string_length) {
    D2D1_RECT_F rect;
    rect.left = 0.0f;
    rect.top = 0.0f;
    rect.right = 512;
    rect.bottom = 512;
    
    // https://learn.microsoft.com/en-us/windows/win32/directwrite/getting-started-with-directwrite
    dwrite_state->render_target->BeginDraw();
    dwrite_state->render_target->SetTransform(D2D1::IdentityMatrix());
    dwrite_state->render_target->DrawText(wchar, string_length, dwrite_state->text_format, 
                                          rect, dwrite_state->colour_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP,
                                          DWRITE_MEASURING_MODE_NATURAL);
    
    dwrite_state->render_target->EndDraw();
}