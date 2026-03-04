#include "MainWindowContent.h"
#include "PluginWindow.h"
#include "IconMenu.hpp"
#include "LanguageManager.hpp"
#include "AudioDeviceSettings.h"

// ============================================================
// Palette — matches original LightHost light-grey system UI
// ============================================================
namespace NP
{
    const Colour bg         { 0xFFE8E8E8 };
    const Colour canvas     { 0xFFECECEC };
    const Colour grid       { 0xFFD8D8D8 };
    const Colour zoneBg     { 0xFFDFDFDF };
    const Colour zoneBorder { 0xFFBBBBBB };
    const Colour zoneHeader { 0xFFD0D0D0 };
    const Colour zoneTitle  { 0xFF333333 };
    const Colour rowBg      { 0xFFCCCCCC };
    const Colour rowText    { 0xFF222222 };
    const Colour nodePlugin { 0xFFDDDDDD };
    const Colour nodeIn     { 0xFFB8CBE0 };
    const Colour nodeOut    { 0xFFD8B8B8 };
    const Colour nodeBorder { 0xFF999999 };
    const Colour nodeText   { 0xFF111111 };
    const Colour nodeHint   { 0xFF888888 };
    const Colour portIn     { 0xFF2980B9 };
    const Colour portOut    { 0xFFE67E22 };
    const Colour wireCol    { 0xFF888888 };
    const Colour wireActive { 0xFFE67E22 };
    const Colour wireBad    { 0xFFCC2222 };
}

// ============================================================
// DPI Scaling — Get user-configured scale factor
// ============================================================
inline float getDPIScaleFactor()
{
    return ScaleSettingsManager::getInstance().getScaleFactor();
}

// ============================================================
// Font Scaling — Combines DPI scaling with language-specific font scaling
// ============================================================
inline float getFontScaleFactor()
{
    return getDPIScaleFactor() * LanguageManager::getInstance().getFontScaling();
}

// ============================================================
// ScaleSettingsDialog - Settings window for scale factor
// ============================================================

class ScaleSettingsContentComponent : public Component
{
public:
    std::function<void()> onScaleChanged;

    ScaleSettingsContentComponent()
    {
        label = std::make_unique<Label>("lbl", LanguageManager::getInstance().getText("scaleFactorTitle"));
        label->setColour(Label::textColourId, Colours::black);
        label->setJustificationType(Justification::centredLeft);
        addAndMakeVisible(label.get());

        combo = std::make_unique<ComboBox>("scaleCombo");
        combo->addItem("100%", 1);
        combo->addItem("125%", 2);
        combo->addItem("150%", 3);
        combo->addItem("200%", 4);

        float cur = ScaleSettingsManager::getInstance().getScaleFactor();
        if      (fabs(cur - 1.00f) < 0.01f) combo->setSelectedId(1, dontSendNotification);
        else if (fabs(cur - 1.25f) < 0.01f) combo->setSelectedId(2, dontSendNotification);
        else if (fabs(cur - 1.50f) < 0.01f) combo->setSelectedId(3, dontSendNotification);
        else if (fabs(cur - 2.00f) < 0.01f) combo->setSelectedId(4, dontSendNotification);
        else                                 combo->setSelectedId(1, dontSendNotification);

        combo->onChange = [this]
        {
            const float scales[] = { 1.0f, 1.25f, 1.5f, 2.0f };
            int id = combo->getSelectedId();
            if (id >= 1 && id <= 4)
            {
                ScaleSettingsManager::getInstance().setScaleFactor(scales[id - 1]);
                if (onScaleChanged) onScaleChanged();
            }
        };
        addAndMakeVisible(combo.get());

        okButton = std::make_unique<TextButton>(LanguageManager::getInstance().getText("OK"));
        okButton->setColour(TextButton::buttonColourId,   Colour::fromRGB(70, 130, 180));
        okButton->setColour(TextButton::textColourOffId,  Colours::white);
        okButton->setColour(TextButton::buttonOnColourId, Colour::fromRGB(40, 100, 150));
        okButton->setColour(TextButton::textColourOnId,   Colours::white);
        okButton->onClick = [this]
        {
            for (auto* p = getParentComponent(); p != nullptr; p = p->getParentComponent())
                if (auto* rw = dynamic_cast<ResizableWindow*>(p))
                    { rw->removeFromDesktop(); delete rw; break; }
        };
        addAndMakeVisible(okButton.get());
    }

    void paint(Graphics& g) override { g.fillAll(Colour::fromRGB(236, 236, 236)); }

    void resized() override
    {
        const float scale = getFontScaleFactor();

        const int pad  = jmax(4, roundToInt(10.0f * scale));
        const int rowH = jmax(18, roundToInt(26.0f * scale));
        const int lblH = jmax(14, roundToInt(18.0f * scale));
        const int gap  = jmax(2, roundToInt(6.0f * scale));
        const int btnW = jmax(50, roundToInt(72.0f * scale));

        auto b = getLocalBounds().reduced(pad);

        label->setFont(Font(FontOptions{}.withHeight(14.0f * scale)));
        label->setBounds(b.removeFromTop(lblH));
        b.removeFromTop(gap);

        combo->setBounds(b.removeFromTop(rowH));
        b.removeFromTop(gap);

        auto btnRow = b.removeFromTop(rowH);
        okButton->setBounds(btnRow.removeFromRight(btnW));
    }

private:
    std::unique_ptr<Label>      label;
    std::unique_ptr<ComboBox>   combo;
    std::unique_ptr<TextButton> okButton;
};

class ScaleSettingsWindow : public ResizableWindow, private Timer
{
public:
    std::function<void()> onScaleChanged;

    ScaleSettingsWindow()
        : ResizableWindow(LanguageManager::getInstance().getText("scaleSettings"),
                          Colour(0xFFECECEC), true)
    {
        auto* content = new ScaleSettingsContentComponent();
        content->onScaleChanged = [this]
        {
            updateWindowSize(/*keepCentre*/ true);
            if (onScaleChanged) onScaleChanged();
        };

        setContentOwned(content, true);
        setUsingNativeTitleBar(true);
        setResizable(false, false);
        setBackgroundColour(Colour::fromRGB(236, 236, 236));

        updateWindowSize(/*keepCentre*/ false);
        lastAppliedScale = getFontScaleFactor();
        startTimerHz(10);
    }

    ~ScaleSettingsWindow() override
    {
        stopTimer();
    }

private:
    float lastAppliedScale = -1.0f;

    void timerCallback() override
    {
        const float cur = getFontScaleFactor();
        if (std::abs(cur - lastAppliedScale) < 0.005f)
            return;

        lastAppliedScale = cur;
        updateWindowSize(/*keepCentre*/ true);
    }

    void updateWindowSize(bool keepCentre)
    {
        const float scale = getFontScaleFactor();
        const int width = jmax(280, roundToInt(320.0f * scale));
        const int height = jmax(120, roundToInt(135.0f * scale));

        if (keepCentre && getWidth() > 0 && getHeight() > 0)
        {
            const auto centre = getBounds().getCentre();
            setSize(width, height);
            setCentrePosition(centre);
        }
        else
        {
            centreWithSize(width, height);
        }
    }
};

// ============================================================
// ScaleSettingsDialog - Settings window for scale factor
// ============================================================
class PluginParameterListener : public AudioProcessorListener
{
public:
    explicit PluginParameterListener(std::function<void()> callback)
        : onParameterChanged(std::move(callback)) {}
    
    void audioProcessorChanged(AudioProcessor*, const ChangeDetails&) override { }
    void audioProcessorParameterChanged(AudioProcessor*, int, float) override
    {
        if (onParameterChanged) onParameterChanged();
    }

private:
    std::function<void()> onParameterChanged;
};

// Global map to hold plugin listeners and keep them alive
static std::map<uint32, std::unique_ptr<PluginParameterListener>> g_pluginListeners;

// ============================================================
// NodeGraphCanvas — constructor
// ============================================================

NodeGraphCanvas::NodeGraphCanvas(AudioDeviceManager& dm,
                                 KnownPluginList& kpl,
                                 AudioPluginFormatManager& fmt,
                                 AudioProcessorGraph& g)
    : deviceManager(dm), knownPlugins(kpl), formatManager(fmt), graph(g)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);  // Enable keyboard focus for Delete key handling
}

// ============================================================
// Geometry helpers
// ============================================================

NodeGraphCanvas::Zone NodeGraphCanvas::zoneAt(Point<int> p) const
{
    if (p.x < getZoneWidth())              return Zone::Left;
    if (p.x > getWidth() - getZoneWidth()) return Zone::Right;
    return Zone::Center;
}

Point<int> NodeGraphCanvas::inputPortPos(const PluginNode& n) const
{
    if (n.type == NodeType::Input)  return { -999, -999 };
    if (n.type == NodeType::Output) { auto b = nodeBounds(n); return { b.getX(), b.getCentreY() }; }
    return n.inputPort();
}

Point<int> NodeGraphCanvas::outputPortPos(const PluginNode& n) const
{
    if (n.type == NodeType::Output) return { -999, -999 };
    if (n.type == NodeType::Input)  { auto b = nodeBounds(n); return { b.getRight(), b.getCentreY() }; }
    return n.outputPort();
}

Rectangle<int> NodeGraphCanvas::nodeBounds(const PluginNode& n) const
{
    if (n.type == NodeType::Input || n.type == NodeType::Output)
    {
        // 計算此節點在同類型中的排位（slot），確保 Y 坐標隨 scale 動態更新
        int slot = 0;
        for (const auto& nd : nodes)
        {
            if (nd.type == n.type)
            {
                if (nd.id == n.id) break;
                ++slot;
            }
        }
        int y = getHeaderHeight() + 6 + slot * (PluginNode::getSideHeight() + 6);
        int x = (n.type == NodeType::Input) ? 0 : getWidth() - getZoneWidth();
        return { x, y, getZoneWidth(), PluginNode::getSideHeight() };
    }
    return n.bounds();
}

// ============================================================
// addNode — visual + audio graph
// ============================================================

void NodeGraphCanvas::addNode(const String& name, NodeType type)
{
    PluginNode n;
    n.id   = nextId++;
    n.type = type;
    n.name = name;

    // Set position
    if (type == NodeType::Input)
    {
        int cnt = 0;
        for (const auto& x : nodes) if (x.type == NodeType::Input) ++cnt;
        n.pos = { 0, getHeaderHeight() + 6 + cnt * (PluginNode::getSideHeight() + 6) };
        n.graphNodeId = AudioProcessorGraph::NodeID(kInputNodeUID);
    }
    else if (type == NodeType::Output)
    {
        int cnt = 0;
        for (const auto& x : nodes) if (x.type == NodeType::Output) ++cnt;
        n.pos = { getWidth() - getZoneWidth(), getHeaderHeight() + 6 + cnt * (PluginNode::getSideHeight() + 6) };
        n.graphNodeId = AudioProcessorGraph::NodeID(kOutputNodeUID);
    }
    // Plugin nodes are added via showPluginPicker, which sets graphNodeId itself.

    nodes.push_back(n);
    if (onGraphChanged) onGraphChanged();
    repaint();
}

// ============================================================
// AudioProcessorGraph helpers
// ============================================================

void NodeGraphCanvas::addGraphConnection(const PluginNode& from, const PluginNode& to)
{
    // Verify that nodes exist in the graph
    if (!graph.getNodeForId(from.graphNodeId)) {
        DBG("WARNING: Source node " << from.graphNodeId.uid << " not found in graph!");
        return;
    }
    if (!graph.getNodeForId(to.graphNodeId)) {
        DBG("WARNING: Target node " << to.graphNodeId.uid << " not found in graph!");
        return;
    }
    
    DBG("Adding connection from " << from.graphNodeId.uid << " to " << to.graphNodeId.uid);
    
    if (!graph.addConnection({ { from.graphNodeId, 0 }, { to.graphNodeId, 0 } })) {
        DBG("WARNING: Failed to add connection (channel 0)");
    }
    if (!graph.addConnection({ { from.graphNodeId, 1 }, { to.graphNodeId, 1 } })) {
        DBG("WARNING: Failed to add connection (channel 1)");
    }
    
    graph.rebuild();  // Rebuild the graph topology to apply new connections
}

void NodeGraphCanvas::removeGraphConnection(const PluginNode& from, const PluginNode& to)
{
    graph.removeConnection({ { from.graphNodeId, 0 }, { to.graphNodeId, 0 } });
    graph.removeConnection({ { from.graphNodeId, 1 }, { to.graphNodeId, 1 } });
    graph.rebuild();  // Rebuild the graph topology after removing connections
}

void NodeGraphCanvas::clearGraphInputConnections(const PluginNode& toNode)
{
    // Remove all existing connections going INTO toNode from the graph
    for (const auto& w : wires)
    {
        if (w.toNode != toNode.id) continue;
        for (const auto& fn : nodes)
        {
            if (fn.id == w.fromNode)
            {
                removeGraphConnection(fn, toNode);
                break;
            }
        }
    }
    // Remove from wire list
    wires.erase(std::remove_if(wires.begin(), wires.end(),
        [&toNode](const NodeWire& w) { return w.toNode == toNode.id; }),
        wires.end());
}

void NodeGraphCanvas::disconnectNode(int nodeId)
{
    // Disconnect all wires connected to this node (both input and output)
    std::vector<int> wiresToRemove;
    
    for (size_t i = 0; i < wires.size(); ++i)
    {
        if (wires[i].fromNode == nodeId || wires[i].toNode == nodeId)
            wiresToRemove.push_back((int)i);
    }
    
    // Remove wires in reverse order to maintain indices
    for (int idx = (int)wiresToRemove.size() - 1; idx >= 0; --idx)
    {
        const auto& w = wires[wiresToRemove[idx]];
        
        // Find the from and to nodes and remove the graph connection
        const PluginNode *frNode = nullptr, *toNode = nullptr;
        for (const auto& nd : nodes)
        {
            if (nd.id == w.fromNode) frNode = &nd;
            if (nd.id == w.toNode)   toNode = &nd;
        }
        
        if (frNode && toNode)
            removeGraphConnection(*frNode, *toNode);
        
        wires.erase(wires.begin() + wiresToRemove[idx]);
    }
    
    graph.rebuild();
    if (onGraphChanged) onGraphChanged();
    repaint();
}

// ============================================================
// Drawing
// ============================================================

void NodeGraphCanvas::drawZoneBackgrounds(Graphics& g) const
{
    const int w = getWidth(), h = getHeight();

    g.setColour(NP::zoneBg);
    g.fillRect(0, 0, getZoneWidth(), h);
    g.setColour(NP::zoneBorder);
    g.drawLine((float)getZoneWidth() - 0.5f, 0, (float)getZoneWidth() - 0.5f, (float)h, 1);

    g.setColour(NP::zoneBg);
    g.fillRect(w - getZoneWidth(), 0, getZoneWidth(), h);
    g.setColour(NP::zoneBorder);
    g.drawLine((float)(w - getZoneWidth()) + 0.5f, 0, (float)(w - getZoneWidth()) + 0.5f, (float)h, 1);

    auto drawHeader = [&](int x, const String& title)
    {
        const Rectangle<int> hdr(x, 0, getZoneWidth(), getHeaderHeight());
        g.setColour(NP::zoneHeader);
        g.fillRect(hdr);
        g.setColour(NP::zoneTitle);
        g.setFont(Font(FontOptions{}.withHeight(13.5f * getFontScaleFactor()).withStyle("Bold")));
        g.drawText(title, hdr, Justification::centred, false);
    };
    drawHeader(0,        LanguageManager::getInstance().getText("inputPorts"));
    drawHeader(w-getZoneWidth(), LanguageManager::getInstance().getText("outputPorts"));

    bool hasIn = false, hasOut = false;
    for (const auto& nd : nodes)
    {
        if (nd.type == NodeType::Input)  hasIn  = true;
        if (nd.type == NodeType::Output) hasOut = true;
    }
    g.setColour(Colour(0xFF999999));
    g.setFont(Font(FontOptions{}.withHeight(11.f * getFontScaleFactor())));
    if (!hasIn)  g.drawText(LanguageManager::getInstance().getText("doubleClickToAdd"), Rectangle<int>(0, getHeaderHeight(), getZoneWidth(), 22),        Justification::centred, false);
    if (!hasOut) g.drawText(LanguageManager::getInstance().getText("doubleClickToAdd"), Rectangle<int>(w-getZoneWidth(), getHeaderHeight(), getZoneWidth(), 22), Justification::centred, false);
}

void NodeGraphCanvas::drawNode(Graphics& g, const PluginNode& n) const
{
    // --- Side-panel style rows ---
    if (n.type == NodeType::Input || n.type == NodeType::Output)
    {
        const bool isInput = (n.type == NodeType::Input);
        const auto b = nodeBounds(n);

        g.setColour(isInput ? NP::nodeIn : NP::nodeOut);
        g.fillRoundedRectangle(b.reduced(6, 2).toFloat(), 4.f);
        g.setColour(NP::nodeBorder);
        g.drawRoundedRectangle(b.reduced(6, 2).toFloat(), 4.f, 1.f);
        
        // Draw selection highlight (yellow border)
        if (selectedNode == n.id)
        {
            g.setColour(Colour(0xFFFFDD00));
            g.drawRoundedRectangle(b.reduced(6, 2).toFloat().expanded(2.f), 4.f, 3.f);
        }

        const auto textRect = isInput
            ? b.reduced(8, 0).withTrimmedRight(16)
            : b.reduced(8, 0).withTrimmedLeft(16);
        g.setColour(NP::rowText);
        g.setFont(Font(FontOptions{}.withHeight(12.f * getFontScaleFactor())));
        g.drawText(n.name, textRect, Justification::centredLeft, true);

        // Port dot on inner edge
        const auto portPt = isInput ? outputPortPos(n) : inputPortPos(n);
        const float pr = (float)PluginNode::getPortRadius();
        g.setColour(isInput ? NP::portOut : NP::portIn);
        g.fillEllipse(portPt.x - pr, portPt.y - pr, pr*2, pr*2);
        g.setColour(NP::nodeBorder);
        g.drawEllipse(portPt.x - pr, portPt.y - pr, pr*2, pr*2, 1.f);
        return;
    }

    // --- Floating plugin node ---
    const auto bf = n.bounds().toFloat();
    g.setColour(Colour(0x40000000));
    g.fillRoundedRectangle(bf.translated(2, 2), 6.f);
    g.setColour(NP::nodePlugin);
    g.fillRoundedRectangle(bf, 6.f);
    g.setColour(NP::nodeBorder);
    g.drawRoundedRectangle(bf, 6.f, 1.5f);
    
    // Draw selection highlight (yellow border)
    if (selectedNode == n.id)
    {
        g.setColour(Colour(0xFFFFDD00));
        g.drawRoundedRectangle(bf.expanded(2.f), 6.f, 3.f);
    }

    g.setColour(NP::nodeText);
    g.setFont(Font(FontOptions{}.withHeight(12.f * getFontScaleFactor()).withStyle("Bold")));
    g.drawText(n.name, n.bounds().reduced(PluginNode::getPortRadius() + 4, 0), Justification::centred, true);

    g.setColour(NP::nodeHint);
    g.setFont(Font(FontOptions{}.withHeight(10.f * getFontScaleFactor())));
    g.drawText(LanguageManager::getInstance().getText("doubleClick"), n.bounds().withTrimmedTop(n.bounds().getHeight() / 2 + 2),
               Justification::centred, false);

    auto drawPort = [&](Point<int> pt, Colour col)
    {
        const float pr = (float)PluginNode::getPortRadius();
        g.setColour(col);
        g.fillEllipse(pt.x - pr, pt.y - pr, pr*2, pr*2);
        g.setColour(NP::nodeBorder);
        g.drawEllipse(pt.x - pr, pt.y - pr, pr*2, pr*2, 1.f);
    };
    drawPort(n.inputPort(),  NP::portIn);
    drawPort(n.outputPort(), NP::portOut);
}

void NodeGraphCanvas::drawWire(Graphics& g, Point<int> a, Point<int> b, bool active) const
{
    g.setColour(active ? NP::wireActive : NP::wireCol);
    Path p;
    p.startNewSubPath(a.toFloat());
    const float cx = (a.x + b.x) * 0.5f;
    p.cubicTo(cx, (float)a.y, cx, (float)b.y, (float)b.x, (float)b.y);
    g.strokePath(p, PathStrokeType(2.f));
}

void NodeGraphCanvas::paint(Graphics& g)
{
    g.fillAll(NP::canvas);
    g.setColour(NP::grid);
    for (int x = getZoneWidth(); x < getWidth() - getZoneWidth(); x += 32)
        for (int y = 0; y < getHeight(); y += 32)
            g.fillRect(x, y, 1, 1);

    drawZoneBackgrounds(g);

    // Committed wires
    for (const auto& wire : wires)
    {
        const PluginNode *fr = nullptr, *to = nullptr;
        for (const auto& nd : nodes)
        {
            if (nd.id == wire.fromNode) fr = &nd;
            if (nd.id == wire.toNode)   to = &nd;
        }
        if (fr && to) drawWire(g, outputPortPos(*fr), inputPortPos(*to), false);
    }

    // Live drag
    if (draggingWire && wireFrom >= 0)
    {
        for (const auto& nd : nodes)
        {
            if (nd.id != wireFrom) continue;
            int dummy = -1;
            bool valid;
            Point<int> anchor;
            if (!wireDragFromInput)
            {
                anchor = outputPortPos(nd);
                valid  = nearInputPort(wireCursor, dummy) && isValidWire(wireFrom, dummy);
            }
            else
            {
                anchor = inputPortPos(nd);
                valid  = nearOutputPort(wireCursor, dummy) && isValidWire(dummy, wireFrom);
            }
            g.setColour(valid ? NP::wireActive : NP::wireBad);
            Path p;
            const float cx = (anchor.x + wireCursor.x) * 0.5f;
            p.startNewSubPath(anchor.toFloat());
            p.cubicTo(cx, (float)anchor.y, cx, (float)wireCursor.y,
                      (float)wireCursor.x, (float)wireCursor.y);
            g.strokePath(p, PathStrokeType(2.f));
            break;
        }
    }

    for (const auto& nd : nodes) drawNode(g, nd);

    bool hasPlugin = false;
    for (const auto& nd : nodes) if (nd.type == NodeType::Plugin) { hasPlugin = true; break; }
    if (!hasPlugin)
    {
        g.setColour(Colour(0xFF999999));
        g.setFont(Font(FontOptions{}.withHeight(11.f * getFontScaleFactor())));
        g.drawText(LanguageManager::getInstance().getText("doubleClickToAddPlugin"),
            Rectangle<int>(getZoneWidth(), 0, getWidth()-2*getZoneWidth(), getHeight()),
            Justification::centred, false);
    }
}

// ============================================================
// Hit testing
// ============================================================

int NodeGraphCanvas::nodeAtPoint(Point<int> p) const
{
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it)
        if (nodeBounds(*it).contains(p)) return it->id;
    return -1;
}

bool NodeGraphCanvas::nearOutputPort(Point<int> p, int& outId) const
{
    const int kSnap = PluginNode::getPortRadius() + 6;
    for (const auto& nd : nodes)
        if (nd.hasOutputPort() && outputPortPos(nd).getDistanceFrom(p) <= kSnap)
        { outId = nd.id; return true; }
    return false;
}

bool NodeGraphCanvas::nearInputPort(Point<int> p, int& outId) const
{
    const int kSnap = PluginNode::getPortRadius() + 6;
    for (const auto& nd : nodes)
        if (nd.hasInputPort() && inputPortPos(nd).getDistanceFrom(p) <= kSnap)
        { outId = nd.id; return true; }
    return false;
}

bool NodeGraphCanvas::isValidWire(int fromId, int toId) const
{
    if (fromId == toId) return false;
    const PluginNode *fr = nullptr, *to = nullptr;
    for (const auto& nd : nodes)
    {
        if (nd.id == fromId) fr = &nd;
        if (nd.id == toId)   to = &nd;
    }
    if (!fr || !to) return false;
    if (!fr->hasOutputPort() || !to->hasInputPort())                         return false;
    if (fr->type == NodeType::Input  && to->type == NodeType::Input)         return false;
    if (fr->type == NodeType::Output && to->type == NodeType::Output)        return false;
    return true;
}

// ============================================================
// Mouse events
// ============================================================

void NodeGraphCanvas::mouseDoubleClick(const MouseEvent& e)
{
    const int hit = nodeAtPoint(e.getPosition());
    if (hit >= 0)
    {
        for (const auto& nd : nodes)
            if (nd.id == hit)
            {
                if (nd.type == NodeType::Plugin) openPluginEditor(hit);
                else {
                    if (onEditNode) onEditNode(hit, nd.type);
                }
                return;
            }
        return;
    }
}

void NodeGraphCanvas::mouseDown(const MouseEvent& e)
{
    selectedNode = nodeAtPoint(e.getPosition());
    repaint();
    
    // ================== RIGHT-CLICK: Context Menu ==================
    if (e.mods.isRightButtonDown())
    {
        const int hitNode = nodeAtPoint(e.getPosition());
        const Zone zone = zoneAt(e.getPosition());
        const auto screenPos = localPointToGlobal(e.getPosition());
        
        if (hitNode >= 0)
        {
            // Right-click on a node
            for (const auto& nd : nodes)
            {
                if (nd.id != hitNode) continue;
                
                if (nd.type == NodeType::Input)
                {
                    // Input node: New Input, Disconnect, Delete
                    PopupMenu m;
                    m.addItem(1, LanguageManager::getInstance().getText("addInputDevice"));
                    m.addItem(2, LanguageManager::getInstance().getText("disconnectAllWires"));
                    m.addSeparator();
                    m.addItem(3, LanguageManager::getInstance().getText("delete"));
                    m.showMenuAsync(PopupMenu::Options().withTargetScreenArea({screenPos.x, screenPos.y, 1, 1}),
                        [this, hitNode](int result) {
                            if (result == 1) onDoubleClickLeft();
                            else if (result == 2) disconnectNode(hitNode);
                            else if (result == 3) removeNode(hitNode);
                        });
                }
                else if (nd.type == NodeType::Output)
                {
                    // Output node: New Output, Disconnect, Delete
                    PopupMenu m;
                    m.addItem(1, LanguageManager::getInstance().getText("addOutputDevice"));
                    m.addItem(2, LanguageManager::getInstance().getText("disconnectAllWires"));
                    m.addSeparator();
                    m.addItem(3, LanguageManager::getInstance().getText("delete"));
                    m.showMenuAsync(PopupMenu::Options().withTargetScreenArea({screenPos.x, screenPos.y, 1, 1}),
                        [this, hitNode](int result) {
                            if (result == 1) onDoubleClickRight();
                            else if (result == 2) disconnectNode(hitNode);
                            else if (result == 3) removeNode(hitNode);
                        });
                }
                else if (nd.type == NodeType::Plugin)
                {
                    // Plugin node: New Plugin, Disconnect, Delete
                    PopupMenu m;
                    m.addItem(1, LanguageManager::getInstance().getText("addPlugin"));
                    m.addItem(2, LanguageManager::getInstance().getText("disconnectAllWires"));
                    m.addSeparator();
                    m.addItem(3, LanguageManager::getInstance().getText("delete"));
                    m.showMenuAsync(PopupMenu::Options().withTargetScreenArea({screenPos.x, screenPos.y, 1, 1}),
                        [this, hitNode, ePos = e.getPosition()](int result) {
                            if (result == 1) showPluginPicker(ePos);
                            else if (result == 2) disconnectNode(hitNode);
                            else if (result == 3) removeNode(hitNode);
                        });
                }
                return;
            }
        }
        else
        {
            // Right-click on empty area
            if (zone == Zone::Left)
            {
                PopupMenu m;
                m.addItem(1, LanguageManager::getInstance().getText("addInputDevice"));
                m.showMenuAsync(PopupMenu::Options().withTargetScreenArea({screenPos.x, screenPos.y, 1, 1}),
                    [this](int result) { if (result == 1) onDoubleClickLeft(); });
            }
            else if (zone == Zone::Right)
            {
                PopupMenu m;
                m.addItem(1, LanguageManager::getInstance().getText("addOutputDevice"));
                m.showMenuAsync(PopupMenu::Options().withTargetScreenArea({screenPos.x, screenPos.y, 1, 1}),
                    [this](int result) { if (result == 1) onDoubleClickRight(); });
            }
            else if (zone == Zone::Center)
            {
                // 右鍵在中間區域顯示外掛程式選單
                showPluginPicker(e.getPosition());
            }
        }
        return;
    }
    
    // ================== LEFT-CLICK: Select, Drag, Wire ==================
    
    // Check if clicking near a port (to start wire drag)
    int portNode = -1;
    if (nearOutputPort(e.getPosition(), portNode))
    {
        draggingWire      = true;
        wireDragFromInput = false;
        wireFrom          = portNode;
        wireCursor        = e.getPosition();
        draggingNode      = -1;
        return;
    }
    if (nearInputPort(e.getPosition(), portNode))
    {
        draggingWire      = true;
        wireDragFromInput = true;
        wireFrom          = portNode;
        wireCursor        = e.getPosition();
        draggingNode      = -1;
        return;
    }

    // Not near a port: handle node selection and dragging
    if (selectedNode >= 0)
    {
        // Enable keyboard focus for DEL key
        grabKeyboardFocus();
        
        // For Plugin nodes, prepare to drag
        for (const auto& nd : nodes)
            if (nd.id == selectedNode && nd.type == NodeType::Plugin) 
            { 
                draggingNode = selectedNode; 
                break; 
            }
    }
    draggingWire = false;
}

void NodeGraphCanvas::mouseDrag(const MouseEvent& e)
{
    if (draggingWire) { wireCursor = e.getPosition(); repaint(); return; }
    if (draggingNode >= 0)
    {
        for (auto& nd : nodes)
        {
            if (nd.id != draggingNode) continue;
            const int lo = getZoneWidth();
            const int hi = getWidth() - getZoneWidth() - PluginNode::getWidth();
            
            auto newPos = Point<int>(std::max(lo, std::min(hi, e.x - PluginNode::getWidth() / 2)),
                                     std::max(0,  std::min(getHeight() - PluginNode::getHeight(), e.y - PluginNode::getHeight() / 2)));
            if (nd.pos != newPos)
            {
                nd.pos = newPos;
                if (onGraphChanged) onGraphChanged();
                repaint();
            }
            break;
        }
    }
}

void NodeGraphCanvas::mouseUp(const MouseEvent& e)
{
    if (draggingWire && wireFrom >= 0)
    {
        int target = -1;
        if (!wireDragFromInput)
        {
            if (nearInputPort(e.getPosition(), target) && isValidWire(wireFrom, target))
            {
                // Find from/to nodes
                const PluginNode *frNode = nullptr, *toNode = nullptr;
                for (const auto& nd : nodes)
                {
                    if (nd.id == wireFrom) frNode = &nd;
                    if (nd.id == target)   toNode = &nd;
                }
                if (frNode && toNode)
                {
                    DBG("Wire connection: " << frNode->name << " -> " << toNode->name);
                    clearGraphInputConnections(*toNode);   // remove old wires (visual+audio)
                    addGraphConnection(*frNode, *toNode);  // add new audio connection
                    wires.push_back({ wireFrom, target }); // add visual wire
                    if (onGraphChanged) onGraphChanged();
                }
            }
        }
        else
        {
            if (nearOutputPort(e.getPosition(), target) && isValidWire(target, wireFrom))
            {
                const PluginNode *frNode = nullptr, *toNode = nullptr;
                for (const auto& nd : nodes)
                {
                    if (nd.id == target)   frNode = &nd;
                    if (nd.id == wireFrom) toNode = &nd;
                }
                if (frNode && toNode)
                {
                    DBG("Wire connection: " << frNode->name << " -> " << toNode->name);
                    clearGraphInputConnections(*toNode);
                    addGraphConnection(*frNode, *toNode);
                    wires.push_back({ target, wireFrom });
                    if (onGraphChanged) onGraphChanged();
                }
            }
        }
    }
    draggingWire      = false;
    wireDragFromInput = false;
    wireFrom          = -1;
    draggingNode      = -1;
    repaint();
}

// ============================================================
// Plugin picker
// ============================================================

void NodeGraphCanvas::showPluginPicker(Point<int> canvasPos)
{
    PopupMenu m;
    auto types = knownPlugins.getTypes();
    if (!types.isEmpty())
    {
        m.addSectionHeader(LanguageManager::getInstance().getText("availablePlugins"));
        for (int i = 0; i < types.size(); ++i)
            m.addItem(i + 1, types[i].name + "  [" + types[i].pluginFormatName + "]");
        m.addSeparator();
    }
    const int kManage = 100000;
    m.addItem(kManage, LanguageManager::getInstance().getText("addManagePlugins"));

    const auto sc = localPointToGlobal(canvasPos);
    m.showMenuAsync(PopupMenu::Options().withTargetScreenArea({sc.x, sc.y, 1, 1}),
        [this, types, canvasPos, kManage](int result)
        {
            if (result == kManage && onManagePlugins) { onManagePlugins(); return; }
            if (result <= 0 || result > types.size()) return;

            const auto& desc = types[result - 1];

            String err;
            double sr = 44100.0;
            int    bs = 512;
            if (auto* dev = deviceManager.getCurrentAudioDevice())
            {
                sr = dev->getCurrentSampleRate();
                bs = dev->getCurrentBufferSizeSamples();
            }

            auto instance = formatManager.createPluginInstance(desc, sr, bs, err);
            if (!instance)
            {
                AlertWindow::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                    LanguageManager::getInstance().getText("cannotLoadPlugin"), err.isEmpty() ? LanguageManager::getInstance().getText("unknownError") : err);
                return;
            }
            instance->prepareToPlay(sr, bs);

            // addNode() returns Node::Ptr (ReferenceCountedObjectPtr), not Node*
            auto nodePtr = graph.addNode(
                std::unique_ptr<AudioProcessor>(std::move(instance)));
            if (!nodePtr) return;
            
            // Add parameter change listener so we save when plugin params change
            if (auto* proc = nodePtr->getProcessor())
            {
                auto listener = std::make_unique<PluginParameterListener>(
                    [this] { if (onGraphChanged) onGraphChanged(); });
                proc->addListener(listener.get());
                // Store the listener in global map to keep it alive
                g_pluginListeners[nodePtr->nodeID.uid] = std::move(listener);
            }

            PluginNode n;
            n.id          = nextId++;
            n.type        = NodeType::Plugin;
            n.name        = desc.name;
            n.graphNodeId = nodePtr->nodeID;

            const int cx = (getWidth() - getZoneWidth() * 2) / 2 + getZoneWidth() - PluginNode::getWidth() / 2;
            int cnt = 0;
            for (const auto& x : nodes) if (x.type == NodeType::Plugin) ++cnt;
            n.pos = { cx, 60 + cnt * (PluginNode::getHeight() + 20) };

            nodes.push_back(n);
            if (onGraphChanged) onGraphChanged();
            repaint();
        });
}

// ============================================================
// Plugin editor
// ============================================================

void NodeGraphCanvas::openPluginEditor(int nodeId)
{
    const PluginNode* cn = nullptr;
    for (const auto& nd : nodes)
        if (nd.id == nodeId) { cn = &nd; break; }
    if (!cn || cn->type != NodeType::Plugin) return;

    auto* graphNode = graph.getNodeForId(cn->graphNodeId);
    if (!graphNode) return;

    // Open the plugin editor (Normal if available, otherwise Generic)
    PluginWindow::getWindowFor(graphNode, PluginWindow::Normal);
}

// ============================================================
// Remove node
// ============================================================

void NodeGraphCanvas::removeNode(int nodeId)
{
    // Allow removing all node types (Plugin, Input, Output)
    for (const auto& nd : nodes)
    {
        if (nd.id == nodeId)
        {
            // Remove from audio graph if it's in the graph
            if (nd.graphNodeId != AudioProcessorGraph::NodeID(0))
            {
                // Clean up the listener
                g_pluginListeners.erase(nd.graphNodeId.uid);
                if (auto* graphNode = graph.getNodeForId(nd.graphNodeId))
                    graph.removeNode(graphNode);
            }
            
            // Remove from visual nodes
            nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
                [nodeId](const PluginNode& n) { return n.id == nodeId; }),
                nodes.end());
            
            // Remove wires connected to this node
            wires.erase(std::remove_if(wires.begin(), wires.end(),
                [nodeId](const NodeWire& w) { return w.fromNode == nodeId || w.toNode == nodeId; }),
                wires.end());
            
            selectedNode = -1;
            graph.rebuild();  // Rebuild graph after removing node
            if (onGraphChanged) onGraphChanged();
            repaint();
            return;
        }
    }
}

// ============================================================
// Keyboard events
// ============================================================

bool NodeGraphCanvas::keyPressed(const KeyPress& key)
{
    if (key.getKeyCode() == KeyPress::deleteKey && selectedNode >= 0)
    {
        removeNode(selectedNode);
        return true;
    }
    return false;
}

// ============================================================
// MainWindowContent
// ============================================================

MainWindowContent::MainWindowContent(AudioDeviceManager&      dm,
                                     KnownPluginList&          kpl,
                                     AudioPluginFormatManager& fmt,
                                     AudioProcessorGraph&      g)
    : deviceManager(dm), knownPlugins(kpl), formatManager(fmt), graph(g)
{
    // Load scale settings from ApplicationProperties
    ScaleSettingsManager::getInstance().loadSettings();
    
    graphCanvas = std::make_unique<NodeGraphCanvas>(dm, kpl, fmt, g);

    graphCanvas->onDoubleClickLeft  = [this] { showInputDialog();  };
    graphCanvas->onDoubleClickRight = [this] { showOutputDialog(); };
    graphCanvas->onManagePlugins    = [this] { if (onManagePlugins) onManagePlugins(); };
    graphCanvas->onGraphChanged     = [this] { if (onGraphChanged) onGraphChanged(); };
    graphCanvas->onEditNode         = [this] (int /*nodeId*/, NodeType type) {
        auto* wnd = new DeviceSelectorWindow(
            type == NodeType::Input 
                ? LanguageManager::getInstance().getText("audioInput")
                : LanguageManager::getInstance().getText("audioOutput"),
            deviceManager,
            type == NodeType::Input ? 256 : 0,
            type == NodeType::Output ? 256 : 0,
            [this](const String&) { 
                // Save audio device configuration when editing device settings
                std::unique_ptr<XmlElement> audioState(deviceManager.createStateXml());
                getAppProperties().getUserSettings()->setValue("audioDeviceState", audioState.get());
                getAppProperties().saveIfNeeded();
            });
        wnd->addToDesktop(ComponentPeer::windowIsResizable);
        wnd->setVisible(true);
        wnd->toFront(true);
    };

    addAndMakeVisible(*graphCanvas);

    // Create Settings button (added last so it appears on top)
    settingsBtn = std::make_unique<TextButton>(LanguageManager::getInstance().getText("settings"));
    settingsBtn->setColour(TextButton::buttonColourId, Colour::fromRGB(70, 130, 180));
    settingsBtn->setColour(TextButton::textColourOffId, Colours::white);
    settingsBtn->onClick = [this] { showScaleSettings(); };
    settingsBtn->setBounds(8, 8, 70, 28);  // Initial bounds
    addAndMakeVisible(*settingsBtn);
}

void MainWindowContent::showInputDialog()
{
    auto* wnd = new DeviceSelectorWindow(
        LanguageManager::getInstance().getText("audioInput"),
        deviceManager, 256, 0,
        [this](const String& name) { 
            graphCanvas->addNode(name, NodeType::Input);
            // Save audio device configuration
            std::unique_ptr<XmlElement> audioState(deviceManager.createStateXml());
            getAppProperties().getUserSettings()->setValue("audioDeviceState", audioState.get());
            getAppProperties().saveIfNeeded();
        });
    wnd->addToDesktop(ComponentPeer::windowIsResizable);
    wnd->setVisible(true);
    wnd->toFront(true);
}

void MainWindowContent::showOutputDialog()
{
    auto* wnd = new DeviceSelectorWindow(
        LanguageManager::getInstance().getText("audioOutput"),
        deviceManager, 0, 256,
        [this](const String& name) { 
            graphCanvas->addNode(name, NodeType::Output);
            // Save audio device configuration
            std::unique_ptr<XmlElement> audioState(deviceManager.createStateXml());
            getAppProperties().getUserSettings()->setValue("audioDeviceState", audioState.get());
            getAppProperties().saveIfNeeded();
        });
    wnd->addToDesktop(ComponentPeer::windowIsResizable);
    wnd->setVisible(true);
    wnd->toFront(true);
}

void MainWindowContent::showScaleSettings()
{
    // If already open, bring to front instead of opening a second window
    if (scaleSettingsWnd != nullptr)
    {
        scaleSettingsWnd->toFront(true);
        return;
    }
    auto* wnd = new ScaleSettingsWindow();
    scaleSettingsWnd = wnd;  // SafePointer will auto-null when window is deleted
    wnd->onScaleChanged = [this]
    {
        // 縮放改變後：更新主視窗 layout
        resized();
        repaint();
        // 通知外層（IconMenu）調整主視窗大小
        if (onScaleChanged) onScaleChanged();
    };
    wnd->addToDesktop(ComponentPeer::windowIsResizable);
    wnd->setVisible(true);
    wnd->grabKeyboardFocus();
    wnd->toFront(true);
}

void MainWindowContent::paint(Graphics& g) { g.fillAll(NP::bg); }

void MainWindowContent::resized()
{
    auto bounds = getLocalBounds();
    
    // Settings button in bottom-left (scaled with DPI)
    int btnHeight = static_cast<int>(28 * getDPIScaleFactor());
    int btnWidth = static_cast<int>(70 * getDPIScaleFactor());
    int padding = 8;
    
    // Graph canvas fills most of the space
    graphCanvas->setBounds(bounds);
    
    // Settings button at bottom-left (positioned after canvas so it's in front)
    int bottomY = getHeight() - btnHeight - padding;
    settingsBtn->setBounds(padding, bottomY, btnWidth, btnHeight);
}

std::unique_ptr<XmlElement> NodeGraphCanvas::saveState() const
{
    auto xml = std::make_unique<XmlElement>("NodeGraph");
    auto* xNodes = new XmlElement("Nodes");
    xml->addChildElement(xNodes);

    for (const auto& n : nodes)
    {
        auto* xn = new XmlElement("Node");
        xn->setAttribute("id", n.id);
        xn->setAttribute("type", static_cast<int>(n.type));
        xn->setAttribute("name", n.name);
        xn->setAttribute("x", n.pos.x);
        xn->setAttribute("y", n.pos.y);

        if (n.type == NodeType::Plugin)
        {
            if (auto* gNode = graph.getNodeForId(n.graphNodeId))
            {
                if (auto* proc = gNode->getProcessor())
                {
                    PluginDescription desc;
                    if (auto* pi = dynamic_cast<AudioPluginInstance*>(proc))
                        pi->fillInPluginDescription(desc);
                    xn->setAttribute("pluginName", desc.name);
                    xn->setAttribute("pluginFormat", desc.pluginFormatName);
                    xn->setAttribute("pluginFileOrIdentifier", desc.fileOrIdentifier);

                    MemoryBlock mb;
                    proc->getStateInformation(mb);
                    auto* stateXml = new XmlElement("PluginState");
                    stateXml->addTextElement(mb.toBase64Encoding());
                    xn->addChildElement(stateXml);
                }
            }
        }
        xNodes->addChildElement(xn);
    }

    auto* xWires = new XmlElement("Wires");
    xml->addChildElement(xWires);
    for (const auto& w : wires)
    {
        auto* xw = new XmlElement("Wire");
        xw->setAttribute("from", w.fromNode);
        xw->setAttribute("to", w.toNode);
        xWires->addChildElement(xw);
    }

    return xml;
}

void NodeGraphCanvas::loadState(const XmlElement& xml)
{
    // IMPORTANT: Do NOT call graph.clear() here!
    // The INPUT/OUTPUT nodes (UIDs 1000000-1000001) must remain in the graph
    // IconMenu's loadActivePlugins() already created them and we depend on them
    
    nodes.clear();
    wires.clear();

    // The graph must have fixed input and output nodes first (created by IconMenu)
    // We assume they already exist in the graph. We just map them to canvas.

    const auto* xNodes = xml.getChildByName("Nodes");
    if (!xNodes) return;

    for (auto* xn : xNodes->getChildIterator())
    {
        PluginNode n;
        n.id   = xn->getIntAttribute("id");
        n.type = static_cast<NodeType>(xn->getIntAttribute("type"));
        n.name = xn->getStringAttribute("name");
        n.pos  = { xn->getIntAttribute("x"), xn->getIntAttribute("y") };

        if (n.id >= nextId) nextId = n.id + 1;

        if (n.type == NodeType::Input)
            n.graphNodeId = AudioProcessorGraph::NodeID(kInputNodeUID);
        else if (n.type == NodeType::Output)
            n.graphNodeId = AudioProcessorGraph::NodeID(kOutputNodeUID);
        else if (n.type == NodeType::Plugin)
        {
            // Restore plugin
            String pluginName = xn->getStringAttribute("pluginName");
            String formatName = xn->getStringAttribute("pluginFormat");
            String identifier = xn->getStringAttribute("pluginFileOrIdentifier");

            // Look up the full PluginDescription from knownPlugins
            PluginDescription desc;
            desc.fileOrIdentifier = identifier;
            desc.name = pluginName;
            desc.pluginFormatName = formatName;

            // Try to find a better description match in our known list
            for (const auto& d : knownPlugins.getTypes())
            {
                if (d.fileOrIdentifier == identifier)
                {
                    desc = d;
                    break;
                }
            }

            String err;
            double sr = 44100.0;
            int bs = 512;
            if (auto* dev = deviceManager.getCurrentAudioDevice())
            {
                sr = dev->getCurrentSampleRate();
                bs = dev->getCurrentBufferSizeSamples();
            }

            DBG("Restoring plugin: " << desc.name << " [" << desc.pluginFormatName << "]");
            auto instance = formatManager.createPluginInstance(desc, sr, bs, err);
            if (instance)
            {
                if (const auto* xState = xn->getChildByName("PluginState"))
                {
                    MemoryBlock mb;
                    mb.fromBase64Encoding(xState->getAllSubText());
                    instance->setStateInformation(mb.getData(), (int)mb.getSize());
                }
                instance->prepareToPlay(sr, bs);
                auto nodePtr = graph.addNode(std::unique_ptr<AudioProcessor>(std::move(instance)));
                if (nodePtr) 
                {
                    n.graphNodeId = nodePtr->nodeID;
                    
                    // Add parameter change listener so we save when plugin params change
                    if (auto* proc = nodePtr->getProcessor())
                    {
                        auto listener = std::make_unique<PluginParameterListener>(
                            [this] { if (onGraphChanged) onGraphChanged(); });
                        proc->addListener(listener.get());
                        // Store the listener in global map to keep it alive
                        g_pluginListeners[nodePtr->nodeID.uid] = std::move(listener);
                    }
                    
                    DBG("Successfully added plugin node: " << n.name << " ID: " << n.graphNodeId.uid);
                }
                else
                {
                    DBG("FAILED to add plugin node to graph: " << n.name);
                }
            }
            else
            {
                DBG("FAILED to create plugin instance: " << desc.name << " Error: " << err);
            }
        }
        nodes.push_back(n);
    }

    const auto* xWires = xml.getChildByName("Wires");
    if (xWires)
    {
        for (auto* xw : xWires->getChildIterator())
        {
            NodeWire w;
            w.fromNode = xw->getIntAttribute("from");
            w.toNode   = xw->getIntAttribute("to");
            wires.push_back(w);

            const PluginNode *fr = nullptr, *to = nullptr;
            for (const auto& nd : nodes)
            {
                if (nd.id == w.fromNode) fr = &nd;
                if (nd.id == w.toNode)   to = &nd;
            }
            if (fr && to && fr->graphNodeId.uid != 0 && to->graphNodeId.uid != 0)
            {
                DBG("Restoring wire: " << fr->name << " -> " << to->name);
                addGraphConnection(*fr, *to);
            }
        }
    }

    graph.rebuild();
    repaint();
}

std::unique_ptr<XmlElement> MainWindowContent::saveState() const
{
    return graphCanvas->saveState();
}

void MainWindowContent::loadState(const XmlElement& xml)
{
    graphCanvas->loadState(xml);
}
