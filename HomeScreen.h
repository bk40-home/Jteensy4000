// HomeScreen.h
// =============================================================================
// JT-4000 home screen: oscilloscope + 7 section tile grid.
//
// Layout (320 x 240 px):
//   y=0    Header (22px): product name + CPU%
//   y=22   Scope area (88px): waveform + peak meter
//   y=110  Tile grid (124px): 4+3 section tiles
//   y=234  Footer (6px): hint text
//
// Scope redraws every frame (animated).
// Tiles repaint only on touch flash (dirty-flag gated).
// =============================================================================

#pragma once
#include <Arduino.h>
#include "ILI9341_t3n.h"
#include "AudioScopeTap.h"
#include "SynthEngine.h"
#include "TFTSectionTile.h"
#include "JT4000_Sections.h"
#include <math.h>

using SectionSelectedCallback = void (*)(int sectionIndex);

class HomeScreen {
public:
    static constexpr int16_t SW       = 320;
    static constexpr int16_t SH       = 240;
    static constexpr int16_t HEADER_H = 22;
    static constexpr int16_t SCOPE_H  = 88;
    static constexpr int16_t FOOTER_H = 6;
    static constexpr int16_t SCOPE_Y  = HEADER_H;
    static constexpr int16_t TILE_Y   = HEADER_H + SCOPE_H;
    static constexpr int16_t TILE_H   = SH - TILE_Y - FOOTER_H;
    static constexpr int16_t ROW_H    = TILE_H / 2;
    static constexpr int16_t TILE_W   = 78;
    static constexpr int16_t TILE_GAP = 2;
    static constexpr int16_t METER_W  = 26;
    static constexpr int16_t WAVE_W   = SW - METER_W - 4;

    HomeScreen()
        : _display(nullptr), _scopeTap(nullptr), _onSection(nullptr)
        , _highlighted(0), _fullRedraw(true), _tilesDirty(true), _scopeTapped(false)
        // Row 0: sections 0-3  (top row)
        , _t0(1+0*(TILE_W+TILE_GAP), TILE_Y+2,       TILE_W, ROW_H-3, kSections[0])
        , _t1(1+1*(TILE_W+TILE_GAP), TILE_Y+2,       TILE_W, ROW_H-3, kSections[1])
        , _t2(1+2*(TILE_W+TILE_GAP), TILE_Y+2,       TILE_W, ROW_H-3, kSections[2])
        , _t3(1+3*(TILE_W+TILE_GAP), TILE_Y+2,       TILE_W, ROW_H-3, kSections[3])
        // Row 1: sections 4-7  (bottom row — _t7 is the PRESETS tile)
        , _t4(1+0*(TILE_W+TILE_GAP), TILE_Y+ROW_H+1, TILE_W, ROW_H-3, kSections[4])
        , _t5(1+1*(TILE_W+TILE_GAP), TILE_Y+ROW_H+1, TILE_W, ROW_H-3, kSections[5])
        , _t6(1+2*(TILE_W+TILE_GAP), TILE_Y+ROW_H+1, TILE_W, ROW_H-3, kSections[6])
        , _t7(1+3*(TILE_W+TILE_GAP), TILE_Y+ROW_H+1, TILE_W, ROW_H-3, kSections[7])
    {
        // MUST match SECTION_COUNT (currently 8). If SECTION_COUNT changes,
        // add/remove a _tN member AND the corresponding pointer assignment here.
        _tiles[0]=&_t0; _tiles[1]=&_t1; _tiles[2]=&_t2; _tiles[3]=&_t3;
        _tiles[4]=&_t4; _tiles[5]=&_t5; _tiles[6]=&_t6; _tiles[7]=&_t7;
    }

    void begin(ILI9341_t3n* disp, AudioScopeTap* tap, SectionSelectedCallback cb) {
        _display=disp; _scopeTap=tap; _onSection=cb;
        _instance = this;
        for (int i=0; i<SECTION_COUNT; ++i) _tiles[i]->setDisplay(disp);
        _t0.setCallback([]{ if(_instance) _instance->_fire(0); });
        _t1.setCallback([]{ if(_instance) _instance->_fire(1); });
        _t2.setCallback([]{ if(_instance) _instance->_fire(2); });
        _t3.setCallback([]{ if(_instance) _instance->_fire(3); });
        _t4.setCallback([]{ if(_instance) _instance->_fire(4); });
        _t5.setCallback([]{ if(_instance) _instance->_fire(5); });
        _t6.setCallback([]{ if(_instance) _instance->_fire(6); });
        _t7.setCallback([]{ if(_instance) _instance->_fire(7); });  // PRESETS tile
        markFullRedraw();
    }

    void draw(SynthEngine& synth) {
        if (!_display) return;
        if (_fullRedraw) {
            _display->fillScreen(0x0000);
            _drawHeader(synth);
            _drawScope();
            _drawAllTiles();
            _drawFooter();
            _fullRedraw=false; _tilesDirty=false;
            return;
        }
        _drawScope();  // scope always redraws
        if (_tilesDirty) {
            for (int i=0; i<SECTION_COUNT; ++i) _tiles[i]->draw();
            _tilesDirty=false;
        }
    }

    bool onTouch(int16_t x, int16_t y) {
        if (y>=SCOPE_Y && y<SCOPE_Y+SCOPE_H) { _scopeTapped=true; return true; }
        for (int i=0; i<SECTION_COUNT; ++i)
            if (_tiles[i]->onTouch(x,y)) { _tilesDirty=true; return true; }
        return false;
    }

    void onTouchRelease(int16_t x, int16_t y) {
        for (int i=0; i<SECTION_COUNT; ++i) _tiles[i]->onTouchRelease(x,y);
        _tilesDirty=true;
    }

    void onEncoderDelta(int delta) {
        if (!delta) return;
        _highlighted = (_highlighted+delta+SECTION_COUNT) % SECTION_COUNT;
        _tilesDirty=true;
    }

    void onEncoderPress() { _fire(_highlighted); }

    bool isScopeTapped() { bool v=_scopeTapped; _scopeTapped=false; return v; }
    void markFullRedraw() { _fullRedraw=true; }

private:
    static HomeScreen* _instance;
    void _fire(int idx) { if (_onSection) _onSection(idx); }

    void _drawHeader(SynthEngine& synth) {
        _display->fillRect(0,0,SW,HEADER_H,0x1082);
        _display->drawFastHLine(0,HEADER_H-1,SW,0x2104);
        _display->setTextSize(1);
        _display->setTextColor(0x07FF);
        _display->setCursor(4,7); _display->print("JT.4000");
        char buf[16];
        snprintf(buf,sizeof(buf),"CPU:%d%%",(int)AudioProcessorUsageMax());
        _display->setTextColor(0x7BEF);
        _display->setCursor(SW-(int16_t)(strlen(buf)*6)-4,7);
        _display->print(buf);
    }

    void _drawScope() {
        if (!_scopeTap) return;
        _display->fillRect(0,SCOPE_Y,SW,SCOPE_H,0x0000);
        _display->drawRect(0,SCOPE_Y,WAVE_W+2,SCOPE_H,0x1082);

        static int16_t buf[256];
        const uint16_t n = _scopeTap->snapshot(buf,256);

        if (n >= 64) {
            // Rising zero-crossing trigger
            int trig = n/4;
            for (int i=n/4; i<(int)n-32; ++i)
                if (buf[i]<=0 && buf[i+1]>0) { trig=i; break; }

            const int16_t midY = SCOPE_Y + SCOPE_H/2;
            const int spp = ((int)n > WAVE_W) ? (n/WAVE_W) : 1;
            int16_t px=1, py=midY;

            for (int col=0; col<WAVE_W-2; ++col) {
                const int base = trig + col*spp;
                if (base>=(int)n) break;
                // Box-filter average — prevents aliasing on complex waveforms
                int32_t acc=0; int cnt=0;
                for (int s=0; s<spp && (base+s)<(int)n; ++s) { acc+=buf[base+s]; cnt++; }
                const int16_t samp = cnt ? (int16_t)(acc/cnt) : 0;
                // 80% of wave height to keep clipping visible
                int cy = midY - (int)((int32_t)samp*(SCOPE_H-8)*4/(32767*5));
                cy = constrain(cy, SCOPE_Y+2, SCOPE_Y+SCOPE_H-2);
                if (col>0) _display->drawLine(px,py,col+1,cy,0x07E0);
                px=col+1; py=cy;
            }
            _display->drawFastHLine(1,midY,WAVE_W-2,0x1082); // zero line
        } else {
            _display->setTextSize(1); _display->setTextColor(0x2104);
            _display->setCursor(10,SCOPE_Y+SCOPE_H/2-4);
            _display->print("arming...");
        }

        // Peak meter — right strip
        const int16_t mx=WAVE_W+4, mh=SCOPE_H-6;
        const float peak = _scopeTap->readPeakAndClear();
        const float db   = (peak>0.001f) ? 20.0f*log10f(peak) : -60.0f;
        const float dbc  = constrain(db,-60.0f,0.0f);
        const int16_t fill = (int16_t)(((dbc+60.0f)/60.0f)*mh);
        _display->fillRect(mx,SCOPE_Y+3,METER_W-2,mh,0x0000);
        _display->drawRect(mx,SCOPE_Y+3,METER_W-2,mh,0x2104);
        if (fill>0) {
            const int16_t g18=(int16_t)(mh*42/60), g6=(int16_t)(mh*54/60);
            const int16_t gf=min(fill,g18);
            const int16_t yf=max((int16_t)0,(int16_t)(min(fill,g6)-g18));
            const int16_t rf=max((int16_t)0,(int16_t)(fill-g6));
            if(gf>0) _display->fillRect(mx+2,SCOPE_Y+3+mh-gf,      METER_W-4,gf,0x07E0);
            if(yf>0) _display->fillRect(mx+2,SCOPE_Y+3+mh-g18-yf,  METER_W-4,yf,0xFFE0);
            if(rf>0) _display->fillRect(mx+2,SCOPE_Y+3+mh-g18-yf-rf,METER_W-4,rf,0xF800);
        }
        _display->setTextSize(1); _display->setTextColor(0x4208);
        _display->setCursor(mx+METER_W,SCOPE_Y+3);     _display->print("0");
        _display->setCursor(mx+METER_W,SCOPE_Y+3+mh/2);_display->print("-30");
    }

    void _drawAllTiles() {
        _display->fillRect(0,TILE_Y,SW,TILE_H,0x0000);
        for (int i=0; i<SECTION_COUNT; ++i) { _tiles[i]->markDirty(); _tiles[i]->draw(); }
    }

    void _drawFooter() {
        const int16_t fy=SH-FOOTER_H;
        _display->fillRect(0,fy,SW,FOOTER_H,0x1082);
        _display->setTextSize(1); _display->setTextColor(0x4208);
        _display->setCursor(4,fy+1);
        _display->print("TAP SECTION  HOLD-L:FULLSCOPE");
    }

    ILI9341_t3n*            _display;
    AudioScopeTap*          _scopeTap;
    SectionSelectedCallback _onSection;
    int                     _highlighted;
    bool                    _fullRedraw, _tilesDirty, _scopeTapped;
    // One TFTSectionTile per entry in kSections[]. Count MUST equal SECTION_COUNT.
    // If you add a section, add a _tN member here AND wire it in the constructor.
    TFTSectionTile _t0,_t1,_t2,_t3,_t4,_t5,_t6,_t7;  // 8 tiles = SECTION_COUNT
    TFTSectionTile* _tiles[SECTION_COUNT];
};

HomeScreen* HomeScreen::_instance = nullptr;
