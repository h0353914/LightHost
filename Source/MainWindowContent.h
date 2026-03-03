#pragma once

#include "JuceHeader.h"
#include "AudioDeviceSettings.h"

// ============================================================
// DPI Scaling utility
// ============================================================
inline float getDPIScaleFactor();  // Forward declaration, defined in .cpp
inline float getFontScaleFactor();  // Combined DPI + language font scaling, defined in .cpp

//==============================================================================
enum class NodeType { Input, Output, Plugin };

//==============================================================================
/** A visual + audio node. graphNodeId links this to the AudioProcessorGraph. */
struct PluginNode
{
    int        id   { 0 };
    NodeType   type { NodeType::Plugin };
    String     name;
    Point<int> pos  { 200, 100 };

    /** Corresponding AudioProcessorGraph NodeID (0 = not in graph yet). */
    AudioProcessorGraph::NodeID graphNodeId { 0 };

    // Base sizes (will be scaled by DPI factor)
    static constexpr int kW      = 140;
    static constexpr int kH      = 56;
    static constexpr int kSideH  = 40;
    static constexpr int kPortR  = 7;
    
    // Methods to get scaled sizes
    static int getWidth()    { return static_cast<int>(kW * getDPIScaleFactor()); }
    static int getHeight()   { return static_cast<int>(kH * getDPIScaleFactor()); }
    static int getSideHeight() { return static_cast<int>(kSideH * getDPIScaleFactor()); }
    static int getPortRadius() { return static_cast<int>(kPortR * getDPIScaleFactor()); }

    bool hasInputPort()  const { return type != NodeType::Input;  }
    bool hasOutputPort() const { return type != NodeType::Output; }

    Point<int> inputPort()  const { return { pos.x,              pos.y + getHeight() / 2 }; }
    Point<int> outputPort() const { return { pos.x + getWidth(), pos.y + getHeight() / 2 }; }
    Rectangle<int> bounds() const { return { pos.x, pos.y, getWidth(), getHeight() }; }
};

struct NodeWire { int fromNode { -1 }; int toNode { -1 }; };

//==============================================================================
/**
 * NodeGraphCanvas — visual + audio routing graph.
 *   Left  zone: Input device nodes  (fixed, port on right edge)
 *   Centre zone: Plugin nodes (freely movable, double-click = open editor)
 *   Right zone:  Output device nodes (fixed, port on left edge)
 *
 * All wires immediately update the AudioProcessorGraph.
 */
class NodeGraphCanvas : public Component
{
public:
    // Base sizes (will be scaled by DPI factor)
    static constexpr int kZoneW = 170;
    static constexpr int kHdrH  = 34;
    
    // Methods to get scaled sizes
    static int getZoneWidth()  { return static_cast<int>(kZoneW * getDPIScaleFactor()); }
    static int getHeaderHeight() { return static_cast<int>(kHdrH * getDPIScaleFactor()); }

    static constexpr uint32 kInputNodeUID  = 1000000;
    static constexpr uint32 kOutputNodeUID = 1000001;

    NodeGraphCanvas(AudioDeviceManager&      dm,
                    KnownPluginList&          knownPlugins,
                    AudioPluginFormatManager& fmt,
                    AudioProcessorGraph&      graph);
    ~NodeGraphCanvas() override = default;

    /** Add an Input or Output side-panel node (graphNodeId pre-set to known IDs). */
    void addNode(const String& name, NodeType type);

    const std::vector<PluginNode>& getNodes() const noexcept { return nodes; }

    std::function<void()> onManagePlugins;
    std::function<void()> onDoubleClickLeft;
    std::function<void()> onDoubleClickRight;
    std::function<void(int, NodeType)> onEditNode;
    std::function<void()> onGraphChanged;

    std::unique_ptr<XmlElement> saveState() const;
    void loadState(const XmlElement& xml);

    void paint(Graphics& g) override;
    void mouseDoubleClick(const MouseEvent& e) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    bool keyPressed(const KeyPress& key) override;

private:
    AudioDeviceManager&       deviceManager;
    KnownPluginList&          knownPlugins;
    AudioPluginFormatManager& formatManager;
    AudioProcessorGraph&      graph;

    std::vector<PluginNode> nodes;
    std::vector<NodeWire>   wires;
    int nextId { 1 };

    // Selection state
    int        selectedNode { -1 };

    // Drag state
    int        draggingNode { -1 };
    bool       draggingWire { false };
    bool       wireDragFromInput { false };
    int        wireFrom     { -1 };
    Point<int> wireCursor;

    // ---- Zone geometry -----------------------------------------------
    enum class Zone { Left, Center, Right };
    Zone           zoneAt(Point<int> p) const;
    Point<int>     inputPortPos (const PluginNode& n) const;
    Point<int>     outputPortPos(const PluginNode& n) const;
    Rectangle<int> nodeBounds   (const PluginNode& n) const;

    // ---- Drawing -----------------------------------------------------
    void drawZoneBackgrounds(Graphics& g) const;
    void drawNode(Graphics& g, const PluginNode& n) const;
    void drawWire(Graphics& g, Point<int> a, Point<int> b, bool active) const;

    // ---- Hit testing -------------------------------------------------
    int  nodeAtPoint   (Point<int> p) const;
    bool nearOutputPort(Point<int> p, int& outId) const;
    bool nearInputPort (Point<int> p, int& outId) const;
    bool isValidWire   (int fromId, int toId) const;

    // ---- Graph interaction -------------------------------------------
    /** Connect two canvas nodes in the AudioProcessorGraph (stereo, ch 0+1). */
    void addGraphConnection   (const PluginNode& from, const PluginNode& to);
    /** Disconnect two canvas nodes in the AudioProcessorGraph. */
    void removeGraphConnection(const PluginNode& from, const PluginNode& to);
    /** Remove all graph connections that go into a given node (single-input rule). */
    void clearGraphInputConnections(const PluginNode& to);
    /** Disconnect all wires connected to a node (both input and output). */
    void disconnectNode(int nodeId);

    // ---- Actions -----------------------------------------------------
    void showPluginPicker(Point<int> canvasPos);
    void openPluginEditor(int nodeId);
    void removeNode(int nodeId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeGraphCanvas)
};

//==============================================================================
/** Top-level content. Owns NodeGraphCanvas and shows add-device dialogs. */
class MainWindowContent : public Component
{
public:
    MainWindowContent(AudioDeviceManager&      deviceManager,
                      KnownPluginList&          knownPlugins,
                      AudioPluginFormatManager& formatManager,
                      AudioProcessorGraph&      graph);
    ~MainWindowContent() override = default;

    void resized() override;
    void paint(Graphics& g) override;

    std::function<void()> onManagePlugins;
    std::function<void()> onGraphChanged;

    std::unique_ptr<XmlElement> saveState() const;
    void loadState(const XmlElement& xml);

private:
    AudioDeviceManager&       deviceManager;
    KnownPluginList&          knownPlugins;
    AudioPluginFormatManager& formatManager;
    AudioProcessorGraph&      graph;

    std::unique_ptr<NodeGraphCanvas> graphCanvas;
    std::unique_ptr<TextButton> settingsBtn;
    Component::SafePointer<Component> scaleSettingsWnd;  // Track open scale settings window

    void showInputDialog();
    void showOutputDialog();
    void showScaleSettings();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindowContent)
};
