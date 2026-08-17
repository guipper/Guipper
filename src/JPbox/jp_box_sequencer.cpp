#include "jp_box_sequencer.h"
#include "../JPutils/jp_shader_globals.h"
#include "../JPutils/jp_tooltip.h"
#include <iostream>
#include <algorithm>

JPbox_sequencer::JPbox_sequencer()
    : currentSlot(0)
    , transition(0.0f)
    , slotPreviewSize(55)
    , btnSize(18)
    , slotGap(6)
    , hoveredSlot(-1)
    , hoverStartMillis(0)
    , frameNum(0)
{
}

JPbox_sequencer::~JPbox_sequencer()
{
    clear();
}

void JPbox_sequencer::setup(string _directory, string _name)
{
    JPbox::setup(_directory, _name);
    tipo = SEQUENCERBOX;

    // Make the box bigger
    width = 240;
    height = 180;

    currentSlot = 0;
    transition = 0.0f;
    parameters.addFloatValue(0.0f, "currentIndex");
    parameters.addFloatValue(0.0f, "transition");

    shader.load("shaders/default.vert", "shaders/private/sequencer.frag");

    cout << "SEQUENCER BOX SETUP: " << _name << endl;
}

void JPbox_sequencer::addSlot()
{
    if ((int)slots.size() >= MAX_SLOTS) return;

    string shaderPath = "shaders/private/mix.frag";
    string slotName = name + "_s" + ofToString(slots.size());
    JPbox_shader *shaderBox = new JPbox_shader();
    shaderBox->setup2(shaderPath, slotName);
    shaderBox->setonoff(true);
    slots.push_back(shaderBox);

    if (slots.size() == 1)
        currentSlot = 0;

    cout << "SEQUENCER: added slot " << slots.size() - 1 << endl;
}

void JPbox_sequencer::removeSlot()
{
    if (slots.empty()) return;

    int lastIdx = (int)slots.size() - 1;
    if (slots[lastIdx] != nullptr)
    {
        slots[lastIdx]->clear();
        delete slots[lastIdx];
        slots[lastIdx] = nullptr;
    }
    slots.pop_back();

    if (!slots.empty() && currentSlot >= (int)slots.size())
        currentSlot = (int)slots.size() - 1;
    else if (slots.empty())
        currentSlot = 0;

    cout << "SEQUENCER: removed slot, now " << slots.size() << endl;
}

void JPbox_sequencer::setCurrentSlot(int index)
{
    if (slots.empty()) { currentSlot = 0; return; }
    currentSlot = ofClamp(index, 0, (int)slots.size() - 1);
}

bool JPbox_sequencer::handleClick(float mx, float my)
{
    // Check + button
    if (plusBtnRect.inside(mx, my))
    {
        addSlot();
        return true;
    }
    // Check - button
    if (minusBtnRect.inside(mx, my))
    {
        removeSlot();
        return true;
    }

    // Check slot click
    float contentX = x - width / 2 + padding_leftright + 4;
    float slotAreaY = y - height / 2 + padding_top + 6;
    int cols = max(1, (int)((width - padding_leftright * 2 - 8) / (slotPreviewSize + slotGap)));

    for (int i = 0; i < (int)slots.size(); i++)
    {
        int col = i % cols;
        int row = i / cols;
        float sx = contentX + col * (slotPreviewSize + slotGap);
        float sy = slotAreaY + row * (slotPreviewSize + slotGap + 14);
        ofRectangle slotRect(sx, sy, slotPreviewSize, slotPreviewSize);
        if (slotRect.inside(mx, my))
        {
            setCurrentSlot(i);
            return true;
        }
    }

    return false;
}

void JPbox_sequencer::update()
{
    JPbox::update();

    if (parameters.getSize() > 0)
        currentSlot = (int)parameters.getFloatValue(0);
    if (parameters.getSize() > 1)
        transition = parameters.getFloatValue(1);

    // Update all slot children
    for (int i = 0; i < (int)slots.size(); i++)
    {
        if (slots[i] != nullptr)
            slots[i]->update();
    }

    // The scheduler drops us to the staggered preview rate when nothing on
    // screen depends on this box. The slot children above are updated either
    // way, so their playback and timing keep running.
    if (shouldRenderThisFrame()) updateFBO();
    frameNum++;
}

void JPbox_sequencer::updateFBO()
{
    if (tryPassThroughFBO()) return;

    if (onoff.boolValue)
    {
        ofSetRectMode(OF_RECTMODE_CORNER);
        fbo.begin();
        if (!slots.empty() && shader.isLoaded())
        {
            shader.begin();
            update_globalUniforms();
            update_sequencerUniforms();
            ofRect(0, 0, fbo.getWidth(), fbo.getHeight());
            shader.end();
        }
        else
        {
            ofClear(0, 255);
            ofSetColor(40, 40, 60);
            ofDrawRectangle(0, 0, fbo.getWidth(), fbo.getHeight());
        }
        fbo.end();
    }
    else
    {
        JPbox::updateFBO();
    }
}

void JPbox_sequencer::draw()
{
    ofSetRectMode(OF_RECTMODE_CORNER);
    JPbox::draw();

    float mx = JPdragobject::getMouseX();
    float my = JPdragobject::getMouseY();

    // Auto-resize based on slots
    int cols = max(1, (int)((width - padding_leftright * 2 - 8) / (slotPreviewSize + slotGap)));
    int rows = max(1, (int)ceil((float)slots.size() / cols));
    float gridH = rows * (slotPreviewSize + slotGap + 14) + btnSize + 20;
    float neededH = padding_top + padding_bottom + gridH;
    if (neededH > height)
    {
        height = neededH;
        JPdragobject::setup(x, y, width, height);
    }

    float contentX = x - width / 2 + padding_leftright + 4;
    float slotAreaY = y - height / 2 + padding_top + 6;
    float contentW = width - padding_leftright * 2 - 8;

    // Draw slots grid
    for (int i = 0; i < (int)slots.size(); i++)
    {
        int col = i % cols;
        int row = i / cols;
        float sx = contentX + col * (slotPreviewSize + slotGap);
        float sy = slotAreaY + row * (slotPreviewSize + slotGap + 14);

        // Active slot highlight
        if (i == currentSlot)
        {
            ofSetColor(50, 180, 50);
            ofNoFill();
            ofSetLineWidth(2);
            ofDrawRectRounded(sx - 2, sy - 2, slotPreviewSize + 4, slotPreviewSize + 4, 4);
            ofFill();
        }

        // Slot preview
        ofSetColor(30, 30, 45);
        ofDrawRectRounded(sx, sy, slotPreviewSize, slotPreviewSize, 4);

        if (slots[i] != nullptr)
        {
            ofSetColor(255);
            slots[i]->fbo.draw(sx, sy, slotPreviewSize, slotPreviewSize);
        }
		jp_tooltip::draw("Select sequencer slot " + ofToString(i), sx, sy, slotPreviewSize, slotPreviewSize);

        // Slot number below
        ofSetColor(160);
        jp_constants::p_font.drawString(ofToString(i),
            sx + slotPreviewSize / 2 - jp_constants::p_font.stringWidth(ofToString(i)) / 2,
            sy + slotPreviewSize + 11);
    }

    // Draw + and - buttons at the bottom
    float btnY = slotAreaY + ((int)slots.size() > 0 ?
        (((int)slots.size() - 1) / cols + 1) * (slotPreviewSize + slotGap + 14) : 0) + 8;

    float btnAreaX = x; // center of box
    float plusX = btnAreaX + 6;
    float minusX = btnAreaX - btnSize - 4;

    // Plus button
    ofSetColor(50, 55, 70);
    ofDrawRectRounded(plusX, btnY, btnSize, btnSize, 3);
    ofSetColor(200);
    float plusLabelW = jp_constants::p_font.stringWidth("+");
    jp_constants::p_font.drawString("+", plusX + btnSize / 2 - plusLabelW / 2, btnY + btnSize / 2 + 5);
	jp_tooltip::draw("Add sequencer slot", plusX, btnY, btnSize, btnSize);

    // Minus button
    ofSetColor(50, 55, 70);
    if ((int)slots.size() > 0)
        ofSetColor(70, 55, 55); // Reddish when slots exist
    ofDrawRectRounded(minusX, btnY, btnSize, btnSize, 3);
    ofSetColor(200);
    float minusLabelW = jp_constants::p_font.stringWidth("-");
    jp_constants::p_font.drawString("-", minusX + btnSize / 2 - minusLabelW / 2, btnY + btnSize / 2 + 5);
	jp_tooltip::draw("Remove sequencer slot", minusX, btnY, btnSize, btnSize);

    // Store button rects for hit testing (CORNER mode)
    plusBtnRect.set(plusX, btnY, btnSize, btnSize);
    minusBtnRect.set(minusX, btnY, btnSize, btnSize);

    ofSetColor(255);
}

void JPbox_sequencer::clear()
{
    for (int i = (int)slots.size() - 1; i >= 0; i--)
    {
        if (slots[i] != nullptr)
        {
            slots[i]->clear();
            delete slots[i];
            slots[i] = nullptr;
        }
    }
    slots.clear();
    shader.unload();
    JPbox::clear();
}

void JPbox_sequencer::update_globalUniforms()
{
	JPShaderGlobalsCtx ctx;
	ctx.width = fbo.getWidth();
	ctx.height = fbo.getHeight();
	ctx.boxFrameNum = frameNum;
	ctx.feedback = &fbo.getTexture();
	jp_shader_globals::apply(shader, ctx);
}

void JPbox_sequencer::update_sequencerUniforms()
{
    shader.setUniform1f("currentIndex", (float)currentSlot);
    shader.setUniform1f("transition", transition);
    shader.setUniform1i("numSlots", (int)slots.size());

    for (int i = 0; i < MAX_SLOTS; i++)
    {
        string texName = "slot" + ofToString(i);
        if (i < (int)slots.size() && slots[i] != nullptr)
            shader.setUniformTexture(texName, slots[i]->fbo.getTexture(), i + 1);
        else
            shader.setUniformTexture(texName, fbo.getTexture(), i + 1);
    }
}
