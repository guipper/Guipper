#include "jp_text_render.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#ifdef __linux__
#include <pango/pangocairo.h>
#include <fontconfig/fontconfig.h>
#endif
namespace jp_text {
std::vector<std::string> families() {
    std::vector<std::string> result;
#ifdef __linux__
    PangoFontFamily **list=nullptr;int count=0;
    pango_font_map_list_families(pango_cairo_font_map_get_default(),&list,&count);
    for(int i=0;i<count;++i)result.emplace_back(pango_font_family_get_name(list[i]));
    g_free(list);
#else
    result={"Sans","Serif","Monospace"};
#endif
    std::sort(result.begin(),result.end());return result;
}
bool registerFont(const std::string &path,std::string &family) {
#ifdef __linux__
    int count=0;auto *pattern=FcFreeTypeQuery(reinterpret_cast<const FcChar8*>(path.c_str()),0,nullptr,&count);
    if(!pattern)return false;
    FcChar8 *name=nullptr;bool ok=FcPatternGetString(pattern,FC_FAMILY,0,&name)==FcResultMatch;
    if(ok)family=reinterpret_cast<const char*>(name);
    FcPatternDestroy(pattern);
    if(!ok || !FcConfigAppFontAddFile(FcConfigGetCurrent(),reinterpret_cast<const FcChar8*>(path.c_str())))return false;
    pango_font_map_changed(pango_cairo_font_map_get_default());return true;
#else
    return false;
#endif
}
Raster rasterize(const std::string &text,const std::string &family,const Style &s,int width,int height) {
    Raster result;
#ifdef __linux__
    if(width<1||height<1||width>8192||height>8192 || uint64_t(width)*height>33554432) {result.error="Resolucion de texto no compatible";return result;}
    result.width=width;result.height=height;
    auto *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width,height);
    auto *cr=cairo_create(surface);
    auto *layout=pango_cairo_create_layout(cr);
    auto *font=pango_font_description_new();
    pango_font_description_set_family(font,family.c_str());
    pango_font_description_set_weight(font,PangoWeight(std::clamp(s.weight,100,900)));
    pango_font_description_set_style(font,s.italic?PANGO_STYLE_ITALIC:PANGO_STYLE_NORMAL);
    pango_font_description_set_absolute_size(font,std::clamp(s.size,8.f,512.f)*PANGO_SCALE);
    pango_layout_set_font_description(layout,font);pango_font_description_free(font);
    const float block=std::max(1.f,width*std::clamp(s.width,.05f,1.f)-2*s.margin);
    pango_layout_set_width(layout,int(block*PANGO_SCALE));
    pango_layout_set_wrap(layout,PANGO_WRAP_WORD_CHAR);
    pango_layout_set_alignment(layout,s.align==0?PANGO_ALIGN_LEFT:s.align==2?PANGO_ALIGN_RIGHT:PANGO_ALIGN_CENTER);
    pango_layout_set_line_spacing(layout,s.leading);
    auto *attributes=pango_attr_list_new();pango_attr_list_insert(attributes,pango_attr_letter_spacing_new(int(s.tracking*PANGO_SCALE)));
    pango_layout_set_attributes(layout,attributes);pango_attr_list_unref(attributes);
    pango_layout_set_text(layout,text.c_str(),int(text.size()));
    PangoRectangle ink,logical;pango_layout_get_pixel_extents(layout,&ink,&logical);
    float left=width*s.x-(block+2*s.margin)*.5f+s.margin;
    float top=height*s.y-(s.anchor==0?0:s.anchor==2?logical.height:logical.height*.5f);
    const float pad=s.margin+s.outline;
    result.clipped=left+ink.x-pad<0 || left+ink.x+ink.width+pad>width || top+ink.y-pad<0 || top+ink.y+ink.height+pad>height;
    auto color=[&](const std::array<float,4>&c){cairo_set_source_rgba(cr,c[0],c[1],c[2],c[3]);};
    if(!text.empty()) {
        color(s.background);cairo_rectangle(cr,left-s.margin,top-s.margin,block+2*s.margin,logical.height+2*s.margin);cairo_fill(cr);
        if(s.shadow>0) {
            // A small separable-looking Gaussian kernel on the vector path avoids
            // repeated full-canvas blur allocations. All passes happen only on edits.
            const int radius=s.shadowBlur>0?2:0;float total=0;
            for(int y=-radius;y<=radius;++y)for(int x=-radius;x<=radius;++x)total+=std::exp(-.5f*(x*x+y*y));
            for(int y=-radius;y<=radius;++y)for(int x=-radius;x<=radius;++x) {
                cairo_move_to(cr,left+s.shadowX+x*s.shadowBlur*.5f,top+s.shadowY+y*s.shadowBlur*.5f);
                cairo_set_source_rgba(cr,0,0,0,s.shadow*std::exp(-.5f*(x*x+y*y))/total);
                pango_cairo_show_layout(cr,layout);
            }
        }
        cairo_move_to(cr,left,top);pango_cairo_layout_path(cr,layout);
        if(s.outline>0){color(s.edge);cairo_set_line_join(cr,CAIRO_LINE_JOIN_ROUND);cairo_set_line_width(cr,2*s.outline);cairo_stroke_preserve(cr);}
        color(s.color);cairo_fill(cr);
    }
    g_object_unref(layout);cairo_destroy(cr);cairo_surface_flush(surface);
    if(cairo_surface_status(surface)!=CAIRO_STATUS_SUCCESS){result.error="No se pudo renderizar el texto";cairo_surface_destroy(surface);return result;}
    result.rgba.resize(size_t(width)*height*4);
    const auto *data=cairo_image_surface_get_data(surface);const int stride=cairo_image_surface_get_stride(surface);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        uint32_t pixel=reinterpret_cast<const uint32_t*>(data+y*stride)[x];
        unsigned a=pixel>>24;size_t i=(size_t(y)*width+x)*4;
        for(int c=0;c<3;++c){unsigned v=(pixel>>(16-8*c))&255;result.rgba[i+c]=a?std::min(255u,(v*255+a/2)/a):0;}
        result.rgba[i+3]=a;
    }
    cairo_surface_destroy(surface);
#else
    result.error="native fallback";
#endif
    return result;
}
}
