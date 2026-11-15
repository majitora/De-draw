#pragma once
#include "PluginProcessor.h"
#include "BinaryData.h"
#include <cmath>

// de:draw — pixel / archive look. Everything is drawn in a 1100x660 logical space.
class DeDrawContent : public juce::Component, private juce::Timer
{
public:
    explicit DeDrawContent (DeDrawProcessor& p) : proc (p)
    {
        const int dw[8] = { 58, 50, 50, 50, 66, 66, 66, 44 };
        int x = 190;
        for (int i = 0; i < 8; ++i) { rDur[i] = juce::Rectangle<int> (x, 29, dw[i], 30); x += dw[i] + 6; }
        rMs = juce::Rectangle<int> (690, 29, 76, 30);
        rSnap = juce::Rectangle<int> (770, 29, 94, 30);
        const int tw[3] = { 58, 66, 66 };
        x = 874;
        for (int i = 0; i < 3; ++i) { rTool[i] = juce::Rectangle<int> (x, 29, tw[i], 30); x += tw[i] + 6; }
        const int bw[3] = { 80, 56, 110 };
        x = 50;
        for (int i = 0; i < 3; ++i) { rTab[i] = juce::Rectangle<int> (x, 314, bw[i], 30); x += bw[i] + 8; }
        rBars = juce::Rectangle<int> (50, 352, 1000, 130);
        rPan = juce::Rectangle<int> (50, 510, 1000, 50);
        const int fx[3] = { 50, 228, 406 };
        for (int i = 0; i < 3; ++i) rFx[i] = juce::Rectangle<int> (fx[i], 578, 170, 36);
        rFb = juce::Rectangle<int> (590, 578, 84, 36);
        rMeter[0] = juce::Rectangle<int> (684, 586, 74, 20);
        rMeter[1] = juce::Rectangle<int> (806, 586, 74, 20);
        rMeter[2] = juce::Rectangle<int> (928, 586, 74, 20);
        buildStatic();
        setSize (1100, 660);
        startTimerHz (25);
    }

    void paint (juce::Graphics& g) override
    {
        g.drawImageAt (bg, 0, 0);
        const juce::Colour lc = lightColour();
        drawLight (g, lc);

        juce::ColourGradient shade (juce::Colour (11, 10, 8).withAlpha (0.7f), 0.0f, 0.0f,
                                    juce::Colour (11, 10, 8).withAlpha (0.9f), 0.0f, 660.0f, false);
        shade.addColour (0.22, juce::Colours::transparentBlack);
        shade.addColour (0.38, juce::Colours::transparentBlack);
        shade.addColour (0.62, juce::Colour (11, 10, 8).withAlpha (0.9f));
        g.setGradientFill (shade);
        g.fillRect (0, 0, 1100, 660);

        // top bar
        shadowText (g, "de:draw", juce::Rectangle<int> (24, 18, 170, 50), mono (28.0f, true), juce::Justification::centredLeft);
        static const char* durNames[8] = { "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bar", "4 bar", "ms" };
        const int di = (int) pv ("dur");
        for (int i = 0; i < 8; ++i) pbtn (g, rDur[i], durNames[i], i == di);
        if (di == 7) pbtn (g, rMs, juce::String ((int) pv ("ms")) + " ms", false);
        pbtn (g, rSnap, "Snap: 32", false);
        static const char* toolNames[3] = { "Bars", "Brush", "Erase" };
        for (int i = 0; i < 3; ++i) pbtn (g, rTool[i], toolNames[i], i == tool);

        // rec label
        if ((tick / 12) % 2 == 0) { g.setColour (juce::Colour (0xffc98f84)); g.fillEllipse (24.0f, 90.0f, 10.0f, 10.0f); }
        shadowText (g, "REC  ARCHIVE_2331.JPG", juce::Rectangle<int> (42, 84, 400, 22), mono (18.0f), juce::Justification::centredLeft);

        // panel
        g.setColour (juce::Colours::black);
        g.fillRect (30, 306, 1052, 340);
        g.setColour (juce::Colour (14, 11, 8).withAlpha (0.88f));
        g.fillRect (24, 300, 1052, 340);
        g.setColour (kBorder);
        g.drawRect (24, 300, 1052, 340, 4);

        static const char* tabNames[3] = { "Volume", "Pan", "FX amount" };
        for (int i = 0; i < 3; ++i) pbtn (g, rTab[i], tabNames[i], i == layer);
        g.setColour (kDim);
        g.setFont (mono (16.0f));
        g.drawText ("Draw: each bar = one echo.  Right-click = erase.  FX: click = type, drag = amount",
                    juce::Rectangle<int> (330, 314, 720, 30), juce::Justification::centredRight, true);

        drawBars (g, lc);

        g.setColour (kDim);
        g.setFont (mono (16.0f));
        g.drawText ("0", juce::Rectangle<int> (50, 488, 60, 18), juce::Justification::centredLeft);
        g.drawText ("delay time ->", juce::Rectangle<int> (400, 488, 300, 18), juce::Justification::centred);
        static const char* durLong[8] = { "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bar", "4 bar", "free" };
        g.drawText (durLong[di], juce::Rectangle<int> (940, 488, 110, 18), juce::Justification::centredRight);

        drawPanStrip (g, lc);

        // bottom row
        static const char* typeNames[4] = { "Off", "Bitcrusher", "Distortion", "Lowpass" };
        static const char* typeIds[3] = { "fx1t", "fx2t", "fx3t" };
        static const char* amtIds[3] = { "fx1a", "fx2a", "fx3a" };
        for (int s = 0; s < 3; ++s)
        {
            const int t = juce::jlimit (0, 3, (int) pv (typeIds[s]));
            pbtn (g, rFx[s], "", false);
            g.setColour (typeCol (t));
            g.fillRect (rFx[s].getX() + 10, rFx[s].getY() + 12, 12, 12);
            g.setColour (kText);
            g.setFont (mono (17.0f));
            g.drawText ("FX" + juce::String (s + 1) + " " + typeNames[t],
                        rFx[s].withTrimmedLeft (28), juce::Justification::centredLeft, true);
            g.setColour (typeCol (t));
            g.fillRect (rFx[s].getX() + 6, rFx[s].getBottom() - 6, (int) ((rFx[s].getWidth() - 12) * pv (amtIds[s])), 3);
        }
        const bool fbOn = pv ("fbOn") > 0.5f;
        pbtn (g, rFb, fbOn ? "FB ON" : "FB OFF", fbOn);
        static const char* mids[3] = { "fb", "mix", "out" };
        for (int m = 0; m < 3; ++m)
        {
            const float val = proc.apvts.getParameter (mids[m])->getValue();
            const int filled = (int) std::round (val * 7.0f);
            for (int k = 0; k < 7; ++k)
            {
                g.setColour (k < filled ? lc : juce::Colour (0xff3a342b));
                g.fillRect (rMeter[m].getX() + k * 11, rMeter[m].getY() + 4, 8, 12);
            }
        }
        g.setColour (kDim);
        g.setFont (mono (16.0f));
        g.drawText ("MIX", juce::Rectangle<int> (768, 586, 36, 20), juce::Justification::centredLeft);
        g.drawText ("OUT", juce::Rectangle<int> (890, 586, 36, 20), juce::Justification::centredLeft);

        g.setColour (kText.withAlpha (0.5f));
        g.setFont (mono (16.0f));
        g.drawText ("by maji", juce::Rectangle<int> (900, 641, 176, 18), juce::Justification::centredRight);

        g.drawImageAt (ov, 0, 0);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto pt = e.getPosition();
        dragMode = 0; lastCol = -1; dragMoved = false;
        for (int i = 0; i < 8; ++i)
            if (rDur[i].contains (pt)) { setChoice ("dur", i); return; }
        if (((int) pv ("dur")) == 7 && rMs.contains (pt))
        {
            beginGesture ("ms"); dragMode = 1; dragStart = pv ("ms"); return;
        }
        for (int i = 0; i < 3; ++i)
            if (rTool[i].contains (pt)) { tool = i; return; }
        for (int i = 0; i < 3; ++i)
            if (rTab[i].contains (pt)) { layer = i; return; }
        if (rBars.contains (pt)) { dragMode = 2; edit (e); return; }
        if (rPan.contains (pt)) { dragMode = 3; edit (e); return; }
        static const char* typeIds[3] = { "fx1t", "fx2t", "fx3t" };
        static const char* amtIds[3] = { "fx1a", "fx2a", "fx3a" };
        for (int s = 0; s < 3; ++s)
            if (rFx[s].contains (pt))
            {
                fxIdx = s; dragMode = 4; fxRight = e.mods.isRightButtonDown(); dragStart = pv (amtIds[s]);
                beginGesture (amtIds[s]);
                return;
            }
        if (rFb.contains (pt))
        {
            auto* p = proc.apvts.getParameter ("fbOn");
            p->beginChangeGesture();
            p->setValueNotifyingHost (pv ("fbOn") > 0.5f ? 0.0f : 1.0f);
            p->endChangeGesture();
            return;
        }
        static const char* mids[3] = { "fb", "mix", "out" };
        for (int m = 0; m < 3; ++m)
        {
            if (rMeter[m].expanded (6, 6).contains (pt))
            {
                meterIdx = m; dragMode = 5; beginGesture (mids[m]); setMeter (e); return;
            }
        }
        (void) typeIds;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragMode == 1)
        {
            auto* p = proc.apvts.getParameter ("ms");
            const float ms = juce::jlimit (10.0f, 4000.0f, dragStart * std::exp (-(float) e.getDistanceFromDragStartY() * 0.01f));
            p->setValueNotifyingHost (p->convertTo0to1 (ms));
        }
        else if (dragMode == 2 || dragMode == 3) edit (e);
        else if (dragMode == 4)
        {
            if (std::abs (e.getDistanceFromDragStartY()) > 4) dragMoved = true;
            if (dragMoved)
            {
                static const char* amtIds[3] = { "fx1a", "fx2a", "fx3a" };
                const float v = juce::jlimit (0.0f, 1.0f, dragStart - (float) e.getDistanceFromDragStartY() / 100.0f);
                proc.apvts.getParameter (amtIds[fxIdx])->setValueNotifyingHost (v);
            }
        }
        else if (dragMode == 5) setMeter (e);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragMode == 4 && ! dragMoved)
        {
            static const char* typeIds[3] = { "fx1t", "fx2t", "fx3t" };
            const int cur = (int) pv (typeIds[fxIdx]);
            const int next = fxRight ? (cur + 3) % 4 : (cur + 1) % 4;
            auto* p = proc.apvts.getParameter (typeIds[fxIdx]);
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) next));
            p->endChangeGesture();
        }
        endGesture();
        dragMode = 0;
    }

private:
    const juce::Colour kLight { 0xffcdbb9b }, kBorder { 0xffd8cbb0 }, kText { 0xffefe6d6 },
                       kDim { 0xffbfb39d }, kBtn { 0xff15110c };

    DeDrawProcessor& proc;
    juce::Image bg, ov;
    juce::Rectangle<int> rDur[8], rMs, rSnap, rTool[3], rTab[3], rBars, rPan, rFx[3], rFb, rMeter[3];
    int tool = 0, layer = 0, tick = 0;
    int dragMode = 0, lastCol = -1, fxIdx = 0, meterIdx = 0;
    float lastVal = 0.0f, dragStart = 0.0f;
    bool dragMoved = false, fxRight = false;
    juce::RangedAudioParameter* gp = nullptr;

    static juce::Font mono (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }

    float pv (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    void timerCallback() override { ++tick; repaint(); }

    void beginGesture (const char* id) { endGesture(); gp = proc.apvts.getParameter (id); if (gp) gp->beginChangeGesture(); }
    void endGesture() { if (gp) { gp->endChangeGesture(); gp = nullptr; } }

    void setChoice (const char* id, int idx)
    {
        auto* p = proc.apvts.getParameter (id);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
        p->endChangeGesture();
    }

    void setMeter (const juce::MouseEvent& e)
    {
        static const char* mids[3] = { "fb", "mix", "out" };
        const float v = juce::jlimit (0.0f, 1.0f, (float) (e.getPosition().x - rMeter[meterIdx].getX()) / 74.0f);
        proc.apvts.getParameter (mids[meterIdx])->setValueNotifyingHost (v);
    }

    static juce::Colour typeCol (int t)
    {
        switch (t)
        {
            case 1:  return juce::Colour (0xffa9bdb4);
            case 2:  return juce::Colour (0xffc98f84);
            case 3:  return juce::Colour (0xffb3a3c2);
            default: return juce::Colour (0xff8f877a);
        }
    }

    juce::Colour lightColour() const
    {
        juce::Colour c = kLight;
        static const char* typeIds[3] = { "fx1t", "fx2t", "fx3t" };
        static const char* amtIds[3] = { "fx1a", "fx2a", "fx3a" };
        for (int s = 0; s < 3; ++s)
        {
            const int t = (int) pv (typeIds[s]);
            if (t > 0) c = c.interpolatedWith (typeCol (t), 0.5f * pv (amtIds[s]));
        }
        return c;
    }

    void pbtn (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, bool sel)
    {
        g.setColour (juce::Colours::black);
        g.fillRect (r.translated (3, 3));
        g.setColour (sel ? kLight : kBtn);
        g.fillRect (r);
        g.setColour (kBorder);
        g.drawRect (r, 2);
        if (t.isNotEmpty())
        {
            g.setColour (sel ? juce::Colour (0xff1a140c) : kText);
            g.setFont (mono (17.0f));
            g.drawText (t, r, juce::Justification::centred, true);
        }
    }

    void shadowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, const juce::Font& f, juce::Justification j)
    {
        g.setFont (f);
        g.setColour (juce::Colours::black);
        g.drawText (t, r.translated (2, 2), j, false);
        g.setColour (kText);
        g.drawText (t, r, j, false);
    }

    void tint (juce::Image& img)
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < bd.height; ++y)
            for (int x = 0; x < bd.width; ++x)
            {
                auto c = bd.getPixelColour (x, y);
                const float r = c.getFloatRed(), gg = c.getFloatGreen(), b = c.getFloatBlue();
                const float sr = 0.393f * r + 0.769f * gg + 0.189f * b;
                const float sg = 0.349f * r + 0.686f * gg + 0.168f * b;
                const float sb = 0.272f * r + 0.534f * gg + 0.131f * b;
                const float k = 0.78f;
                bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (
                    juce::jlimit (0.0f, 1.0f, (r + (sr - r) * 0.6f) * k),
                    juce::jlimit (0.0f, 1.0f, (gg + (sg - gg) * 0.6f) * k),
                    juce::jlimit (0.0f, 1.0f, (b + (sb - b) * 0.6f) * k), 1.0f));
            }
    }

    void buildStatic()
    {
        juce::Image src = juce::ImageCache::getFromMemory (BinaryData::ref_jpeg, BinaryData::ref_jpegSize);
        bg = juce::Image (juce::Image::ARGB, 1100, 660, true);
        juce::Graphics g (bg);
        g.fillAll (juce::Colour (0xff0b0a08));
        if (src.isValid())
        {
            src = src.convertedToFormat (juce::Image::ARGB);
            tint (src);
            g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
            g.drawImage (src, 0, -220, 1100, 1100, 0, 0, src.getWidth(), src.getHeight());
        }
        ov = juce::Image (juce::Image::ARGB, 1100, 660, true);
        juce::Graphics o (ov);
        o.setColour (juce::Colours::black.withAlpha (0.3f));
        for (int y = 0; y < 660; y += 4) o.fillRect (0, y, 1100, 2);
        juce::ColourGradient vg (juce::Colours::transparentBlack, 550.0f, 330.0f,
                                 juce::Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true);
        vg.addColour (0.55, juce::Colours::transparentBlack);
        o.setGradientFill (vg);
        o.fillRect (0, 0, 1100, 660);
    }

    void drawLight (juce::Graphics& g, juce::Colour lc)
    {
        const float brightness = 0.55f + 0.45f * juce::jlimit (0.0f, 1.0f, proc.level.load() * 2.0f);
        const juce::Point<float> o (814.0f, 238.0f);
        juce::ColourGradient glow (lc.withAlpha (0.8f * brightness), o.x, o.y,
                                   juce::Colours::transparentBlack, o.x + 320.0f, o.y, true);
        g.setGradientFill (glow);
        g.fillRect (0, 0, 1100, 660);

        int last = 0;
        for (int i = 0; i < DeDrawProcessor::kSteps; ++i)
            if (proc.vol[(size_t) i].load() > 0.01f) last = i;
        const float widthF = (float) last / 31.0f;
        const float pi = juce::MathConstants<float>::pi;
        for (int i = 0; i < DeDrawProcessor::kSteps; ++i)
        {
            const float v = proc.vol[(size_t) i].load();
            if (v < 0.01f) continue;
            const float p = proc.pan[(size_t) i].load();
            const float th = pi - 0.55f * p + ((float) i / 31.0f - 0.5f) * 0.45f * widthF;
            const float half = 0.025f + 0.05f * v;
            const float len = 950.0f;
            juce::Path tri;
            tri.startNewSubPath (o);
            tri.lineTo (o.x + len * std::cos (th - half), o.y + len * std::sin (th - half));
            tri.lineTo (o.x + len * std::cos (th + half), o.y + len * std::sin (th + half));
            tri.closeSubPath();
            juce::ColourGradient gr (lc.withAlpha (0.30f * v * brightness), o.x, o.y,
                                     lc.withAlpha (0.0f), o.x + len * std::cos (th), o.y + len * std::sin (th), false);
            g.setGradientFill (gr);
            g.fillPath (tri);
        }
    }

    void drawBars (juce::Graphics& g, juce::Colour lc)
    {
        auto& arr = proc.layerArr (layer);
        if (layer == 1)
        {
            g.setColour (kDim.withAlpha (0.6f));
            for (int x = 50; x < 1050; x += 12) g.fillRect (x, 416, 6, 2);
        }
        for (int i = 0; i < DeDrawProcessor::kSteps; ++i)
        {
            const float v = arr[(size_t) i].load();
            const float x = std::floor (50.0f + (float) i * 31.25f);
            if (layer == 1)
            {
                const float h = std::abs (v) * 65.0f;
                if (h < 2.0f) continue;
                g.setColour (lc.withAlpha (0.55f + 0.45f * std::abs (v)));
                for (float d = 0.0f; d < h; d += 10.0f)
                {
                    const float seg = std::min (7.0f, h - d);
                    const float y = v < 0.0f ? 417.0f - d - seg : 417.0f + d;
                    g.fillRect (x, y, 22.0f, seg);
                }
            }
            else
            {
                const float h = v * 130.0f;
                if (h < 2.0f) continue;
                g.setColour (lc.withAlpha (0.5f + 0.5f * v));
                for (float d = 0.0f; d < h; d += 10.0f)
                {
                    const float seg = std::min (7.0f, h - d);
                    g.fillRect (x, 482.0f - d - seg, 22.0f, seg);
                }
            }
        }
        g.setColour (kBorder);
        g.fillRect (50, 482, 1000, 4);
    }

    void drawPanStrip (juce::Graphics& g, juce::Colour lc)
    {
        g.setColour (kBorder.withAlpha (0.07f));
        g.fillRect (rPan);
        g.setColour (kDim.withAlpha (0.8f));
        for (int x = 50; x < 1050; x += 12) g.fillRect (x, 534, 6, 2);
        for (int i = 0; i < DeDrawProcessor::kSteps; ++i)
        {
            const float p = proc.pan[(size_t) i].load();
            const bool active = proc.vol[(size_t) i].load() > 0.003f;
            g.setColour (lc.withAlpha (active ? 1.0f : 0.25f));
            const float cx = std::floor (50.0f + (float) i * 31.25f) + 11.0f;
            g.fillRect (cx - 6.0f, 535.0f + p * 19.0f - 6.0f, 12.0f, 12.0f);
        }
        g.setColour (kDim);
        g.setFont (mono (14.0f));
        g.drawText ("L / ceiling", juce::Rectangle<int> (900, 511, 146, 14), juce::Justification::centredRight);
        g.drawText ("R / floor", juce::Rectangle<int> (900, 545, 146, 14), juce::Justification::centredRight);
    }

    void edit (const juce::MouseEvent& e)
    {
        const auto pos = e.getPosition();
        const bool strip = (dragMode == 3);
        const int col = juce::jlimit (0, DeDrawProcessor::kSteps - 1, (int) std::floor ((pos.x - 50) / 31.25f));
        const int target = strip ? 1 : layer;
        float val;
        if (target == 1) val = juce::jlimit (-1.0f, 1.0f, strip ? ((float) pos.y - 535.0f) / 19.0f : ((float) pos.y - 417.0f) / 65.0f);
        else             val = juce::jlimit (0.0f, 1.0f, (482.0f - (float) pos.y) / 130.0f);
        if (e.mods.isRightButtonDown() || (! strip && tool == 2)) val = 0.0f;
        auto& arr = proc.layerArr (target);
        if (tool == 1 && lastCol >= 0 && lastCol != col)
        {
            const int a = std::min (lastCol, col), b = std::max (lastCol, col);
            for (int c = a; c <= b; ++c)
            {
                const float t = (float) (c - lastCol) / (float) (col - lastCol);
                arr[(size_t) c].store (lastVal + (val - lastVal) * t);
            }
        }
        else arr[(size_t) col].store (val);
        lastCol = col; lastVal = val;
    }
};

class DeDrawEditor : public juce::AudioProcessorEditor
{
public:
    explicit DeDrawEditor (DeDrawProcessor& p) : juce::AudioProcessorEditor (&p), content (p)
    {
        addAndMakeVisible (content);
        content.setBounds (0, 0, 1100, 660);
        content.setTransform (juce::AffineTransform::scale (0.8f));
        setSize (880, 528);
        setResizable (false, false);
    }
    void resized() override {}

private:
    DeDrawContent content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeDrawEditor)
};
