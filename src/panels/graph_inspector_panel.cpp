#include "panels/graph_inspector_panel.h"
#include <QVBoxLayout>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QGraphicsLineItem>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <fstream>
#include <unordered_set>
#include <spdlog/spdlog.h>
#include <cmath>

namespace microbotica::panels {

GraphInspectorPanel::GraphInspectorPanel(QWidget* parent)
    : QDockWidget(tr("Graph Inspector"), parent)
{
    setObjectName("GraphInspectorPanel");

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    scene_ = new QGraphicsScene(this);
    view_ = new QGraphicsView(scene_, container);
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setDragMode(QGraphicsView::ScrollHandDrag);

    layout->addWidget(view_);
    setWidget(container);
}

bool GraphInspectorPanel::loadFromFile(const std::string& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        spdlog::warn("GraphInspectorPanel: Cannot open {}", json_path);
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;
        loadFromJson(j);
        return true;
    } catch (const nlohmann::json::exception& e) {
        spdlog::error("GraphInspectorPanel: JSON parse error in {}: {}", json_path, e.what());
        return false;
    }
}

void GraphInspectorPanel::loadFromJson(const nlohmann::json& graph) {
    clear();
    layoutGraph(graph);
    view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
}

void GraphInspectorPanel::clear() {
    scene_->clear();
}

void GraphInspectorPanel::layoutGraph(const nlohmann::json& graph) {
    const double NODE_W = 180.0;
    const double NODE_H = 50.0;
    const double H_SPACING = 60.0;
    const double V_SPACING = 80.0;

    // Parse nodes
    struct NodeInfo {
        std::string name;
        std::string type;
        double x = 0, y = 0;
        QGraphicsRectItem* rect = nullptr;
    };

    std::vector<NodeInfo> nodes;
    std::unordered_map<std::string, size_t> nodeIndex;

    if (graph.contains("nodes") && graph["nodes"].is_array()) {
        for (const auto& n : graph["nodes"]) {
            NodeInfo ni;
            ni.name = n.value("name", "");
            ni.type = n.value("type", "");
            nodeIndex[ni.name] = nodes.size();
            nodes.push_back(std::move(ni));
        }
    }

    if (nodes.empty()) {
        auto* text = scene_->addText("No graph data loaded");
        text->setDefaultTextColor(Qt::gray);
        return;
    }

    // Simple left-to-right layout (topological order assumed from JSON)
    for (size_t i = 0; i < nodes.size(); ++i) {
        nodes[i].x = static_cast<double>(i) * (NODE_W + H_SPACING);
        nodes[i].y = 0;
    }

    // Draw coupling groups as background rectangles
    if (graph.contains("coupling_groups") && graph["coupling_groups"].is_array()) {
        for (const auto& group : graph["coupling_groups"]) {
            if (!group.contains("nodes") || !group["nodes"].is_array()) continue;

            double min_x = 1e9, max_x = -1e9;
            for (const auto& nname : group["nodes"]) {
                auto it = nodeIndex.find(nname.get<std::string>());
                if (it != nodeIndex.end()) {
                    double nx = nodes[it->second].x;
                    if (nx < min_x) min_x = nx;
                    if (nx + NODE_W > max_x) max_x = nx + NODE_W;
                }
            }

            if (min_x < 1e9) {
                auto* bg = scene_->addRect(
                    min_x - 10, -V_SPACING / 2,
                    max_x - min_x + 20, NODE_H + V_SPACING,
                    QPen(QColor(100, 100, 200, 80)),
                    QBrush(QColor(100, 100, 200, 30)));
                bg->setZValue(-1);

                bool subcycling = group.value("subcycling", false);
                if (subcycling) {
                    auto* label = scene_->addText("subcycled");
                    label->setPos(min_x - 5, NODE_H + V_SPACING / 2 - 5);
                    label->setDefaultTextColor(QColor(100, 100, 200));
                    auto f = label->font();
                    f.setPointSize(7);
                    label->setFont(f);
                }
            }
        }
    }

    // Draw nodes
    QFont nameFont("Sans", 9, QFont::Bold);
    QFont typeFont("Sans", 7);

    for (auto& ni : nodes) {
        auto* rect = scene_->addRect(
            ni.x, ni.y, NODE_W, NODE_H,
            QPen(Qt::darkGray, 2),
            QBrush(QColor(60, 60, 80)));
        rect->setFlag(QGraphicsItem::ItemIsSelectable);
        ni.rect = rect;

        auto* nameText = scene_->addText(QString::fromStdString(ni.name), nameFont);
        nameText->setDefaultTextColor(Qt::white);
        nameText->setPos(ni.x + 5, ni.y + 3);

        auto* typeText = scene_->addText(QString::fromStdString(ni.type), typeFont);
        typeText->setDefaultTextColor(QColor(180, 180, 180));
        typeText->setPos(ni.x + 5, ni.y + 25);
    }

    // Draw edges
    auto backEdgeSet = std::unordered_set<std::string>();
    if (graph.contains("back_edges") && graph["back_edges"].is_array()) {
        for (const auto& be : graph["back_edges"]) {
            backEdgeSet.insert(be.get<std::string>());
        }
    }

    if (graph.contains("edges") && graph["edges"].is_array()) {
        for (const auto& e : graph["edges"]) {
            std::string src = e.value("source", "");
            std::string tgt = e.value("target", "");

            auto si = nodeIndex.find(src);
            auto ti = nodeIndex.find(tgt);
            if (si == nodeIndex.end() || ti == nodeIndex.end()) continue;

            double x1 = nodes[si->second].x + NODE_W;
            double y1 = nodes[si->second].y + NODE_H / 2;
            double x2 = nodes[ti->second].x;
            double y2 = nodes[ti->second].y + NODE_H / 2;

            std::string edgeKey = src + " -> " + tgt;
            bool isBackEdge = backEdgeSet.count(edgeKey) > 0;

            QPen pen;
            if (isBackEdge) {
                pen = QPen(QColor(255, 150, 50), 2, Qt::DashLine);
                // Back edges go right-to-left — swap and curve
                std::swap(x1, x2);
                y1 += 15;
                y2 += 15;
            } else {
                pen = QPen(QColor(200, 200, 200), 1.5);
            }

            auto* line = scene_->addLine(x1, y1, x2, y2, pen);
            (void)line;

            // Arrowhead
            double angle = std::atan2(y2 - y1, x2 - x1);
            double arrowSize = 8.0;
            QPolygonF arrow;
            arrow << QPointF(x2, y2)
                  << QPointF(x2 - arrowSize * std::cos(angle - 0.3),
                             y2 - arrowSize * std::sin(angle - 0.3))
                  << QPointF(x2 - arrowSize * std::cos(angle + 0.3),
                             y2 - arrowSize * std::sin(angle + 0.3));
            scene_->addPolygon(arrow, QPen(Qt::NoPen),
                               QBrush(isBackEdge ? QColor(255, 150, 50) : QColor(200, 200, 200)));

            // Edge label (field name)
            std::string field = e.value("field", "");
            if (!field.empty()) {
                auto* label = scene_->addText(QString::fromStdString(field));
                label->setDefaultTextColor(QColor(150, 150, 150));
                auto f = label->font();
                f.setPointSize(7);
                label->setFont(f);
                label->setPos((x1 + x2) / 2 - 20, (y1 + y2) / 2 - 15);
            }
        }
    }
}

} // namespace microbotica::panels
