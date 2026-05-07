#include "panels/graph_inspector_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QGraphicsPathItem>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QPainterPath>
#include <QPainter>
#include <QPixmap>
#include <QImage>
#include <QFileDialog>
#include <QMessageBox>
#include <QWheelEvent>
#include <QToolTip>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <cmath>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace microbotica::panels {

// QGraphicsView subclass with two niceties for desktop and laptop trackpad:
//   - Wheel zoom (no modifier needed). Ctrl+wheel still works for users
//     coming from web-app conventions.
//   - Two-finger trackpad pinch via the standard pixelDelta path.
class GraphInspectorPanel::ZoomView : public QGraphicsView {
public:
    using QGraphicsView::QGraphicsView;

protected:
    void wheelEvent(QWheelEvent* e) override {
        // Either explicit Ctrl+wheel OR plain wheel zooms. This matches
        // Maya/Blender expectations and works on trackpads that don't
        // emit scroll events.
        const double factor = (e->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
        scale(factor, factor);
        e->accept();
    }
};

GraphInspectorPanel::GraphInspectorPanel(QWidget* parent)
    : QDockWidget(tr("Graph Inspector"), parent)
{
    setObjectName("GraphInspectorPanel");

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Toolbar ──
    auto* toolbar = new QWidget(container);
    auto* tb = new QHBoxLayout(toolbar);
    tb->setContentsMargins(4, 2, 4, 2);
    tb->setSpacing(2);

    auto makeBtn = [&](const QString& text, const QString& tip) {
        auto* b = new QToolButton(toolbar);
        b->setText(text);
        b->setToolTip(tip);
        b->setAutoRaise(true);
        return b;
    };
    auto* zoomInBtn  = makeBtn("+", tr("Zoom in (or scroll wheel up)"));
    auto* zoomOutBtn = makeBtn("−", tr("Zoom out (or scroll wheel down)"));
    auto* fitBtn     = makeBtn(tr("Fit"), tr("Fit graph to view"));
    auto* exportBtn  = makeBtn(tr("Export PNG…"),
                                tr("Save the graph as a PNG image"));
    tb->addWidget(zoomInBtn);
    tb->addWidget(zoomOutBtn);
    tb->addWidget(fitBtn);
    tb->addStretch();
    tb->addWidget(exportBtn);
    connect(zoomInBtn,  &QToolButton::clicked, this, &GraphInspectorPanel::zoomIn);
    connect(zoomOutBtn, &QToolButton::clicked, this, &GraphInspectorPanel::zoomOut);
    connect(fitBtn,     &QToolButton::clicked, this, &GraphInspectorPanel::zoomFit);
    connect(exportBtn,  &QToolButton::clicked, this, &GraphInspectorPanel::exportPng);
    layout->addWidget(toolbar);

    // ── View ──
    scene_ = new QGraphicsScene(this);
    view_ = new ZoomView(scene_, container);
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setRenderHint(QPainter::TextAntialiasing);
    // Left-mouse drag pans (the most common interaction). Selection is
    // not particularly useful on a read-only graph anyway.
    view_->setDragMode(QGraphicsView::ScrollHandDrag);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    view_->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    view_->setBackgroundBrush(QBrush(QColor(28, 28, 32)));
    // Show tooltips on hover. ItemUsesExtendedStyleOption is a
    // performance-related Qt6 hint; not needed here, just leave defaults.
    view_->setMouseTracking(true);

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
        spdlog::error("GraphInspectorPanel: JSON parse error in {}: {}",
                      json_path, e.what());
        return false;
    }
}

void GraphInspectorPanel::loadFromJson(const nlohmann::json& graph) {
    clear();
    layoutGraph(graph);
    QRectF r = scene_->itemsBoundingRect().adjusted(-30, -30, 30, 30);
    scene_->setSceneRect(r);
    view_->fitInView(r, Qt::KeepAspectRatio);
}

void GraphInspectorPanel::clear() {
    scene_->clear();
}

void GraphInspectorPanel::zoomIn()  { view_->scale(1.25, 1.25); }
void GraphInspectorPanel::zoomOut() { view_->scale(1.0/1.25, 1.0/1.25); }
void GraphInspectorPanel::zoomFit() {
    view_->fitInView(scene_->itemsBoundingRect().adjusted(-30, -30, 30, 30),
                     Qt::KeepAspectRatio);
}

void GraphInspectorPanel::exportPng() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Graph as PNG"),
        QString("graph.png"),
        tr("PNG image (*.png)"));
    if (path.isEmpty()) return;

    QRectF r = scene_->itemsBoundingRect().adjusted(-20, -20, 20, 20);
    QImage img(static_cast<int>(std::ceil(r.width() * 2)),
                static_cast<int>(std::ceil(r.height() * 2)),
                QImage::Format_ARGB32);
    img.fill(QColor(28, 28, 32));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    scene_->render(&p, QRectF(0, 0, img.width(), img.height()), r);
    p.end();
    if (!img.save(path)) {
        QMessageBox::warning(this, tr("Export failed"),
            tr("Could not write %1").arg(path));
        return;
    }
    spdlog::info("GraphInspectorPanel: exported {} ({} x {})",
                  path.toStdString(), img.width(), img.height());
}

void GraphInspectorPanel::layoutGraph(const nlohmann::json& graph) {
    const double NODE_W = 200.0;
    const double NODE_H = 56.0;
    const double H_SPACING = 90.0;
    const double V_SPACING = 30.0;

    struct NodeInfo {
        std::string name;
        std::string type;
        nlohmann::json params;
        int depth = 0;
        int row = 0;
        double x = 0.0, y = 0.0;
    };

    std::vector<NodeInfo> nodes;
    std::unordered_map<std::string, size_t> nodeIndex;

    if (graph.contains("nodes") && graph["nodes"].is_array()) {
        for (const auto& n : graph["nodes"]) {
            NodeInfo ni;
            ni.name = n.value("name", "");
            ni.type = n.value("type", "");
            if (n.contains("params")) ni.params = n["params"];
            nodeIndex[ni.name] = nodes.size();
            nodes.push_back(std::move(ni));
        }
    }

    if (nodes.empty()) {
        auto* text = scene_->addText(tr("No graph data loaded"));
        text->setDefaultTextColor(Qt::gray);
        return;
    }

    struct EdgeInfo {
        size_t src, tgt;
        std::string field;
        std::string target_field;
        bool back = false;
    };
    std::vector<EdgeInfo> edges;
    if (graph.contains("edges") && graph["edges"].is_array()) {
        for (const auto& e : graph["edges"]) {
            std::string src = e.value("source_node",
                                e.value("source", std::string()));
            std::string tgt = e.value("target_node",
                                e.value("target", std::string()));
            std::string field = e.value("source_field",
                                  e.value("field", std::string()));
            std::string tfield = e.value("target_field", std::string());
            auto si = nodeIndex.find(src);
            auto ti = nodeIndex.find(tgt);
            if (si == nodeIndex.end() || ti == nodeIndex.end()) continue;
            edges.push_back({si->second, ti->second, field, tfield, false});
        }
    }

    // Layered topological layout (Kahn-BFS).
    {
        std::vector<int> indeg(nodes.size(), 0);
        std::vector<std::vector<size_t>> succ(nodes.size());
        for (auto& e : edges) {
            indeg[e.tgt]++;
            succ[e.src].push_back(e.tgt);
        }
        std::vector<int> depth(nodes.size(), 0);
        std::queue<size_t> q;
        std::vector<int> remaining = indeg;
        for (size_t i = 0; i < nodes.size(); ++i)
            if (remaining[i] == 0) q.push(i);
        std::unordered_set<size_t> placed;
        while (!q.empty()) {
            size_t u = q.front(); q.pop();
            placed.insert(u);
            for (size_t v : succ[u]) {
                if (depth[v] < depth[u] + 1) depth[v] = depth[u] + 1;
                if (--remaining[v] == 0) q.push(v);
            }
        }
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (!placed.count(i)) {
                int d = 0;
                for (auto& e : edges) {
                    if (e.tgt == i && placed.count(e.src))
                        d = std::max(d, depth[e.src] + 1);
                }
                depth[i] = d;
            }
        }
        for (size_t i = 0; i < nodes.size(); ++i) nodes[i].depth = depth[i];
        for (auto& e : edges) {
            if (nodes[e.src].depth >= nodes[e.tgt].depth) e.back = true;
        }
    }

    // Bucket by depth → assign coords.
    {
        std::unordered_map<int, std::vector<size_t>> byDepth;
        for (size_t i = 0; i < nodes.size(); ++i)
            byDepth[nodes[i].depth].push_back(i);
        for (auto& [d, v] : byDepth) {
            const double col_x = static_cast<double>(d) * (NODE_W + H_SPACING);
            const double col_h = v.size() * (NODE_H + V_SPACING) - V_SPACING;
            for (size_t r = 0; r < v.size(); ++r) {
                auto& ni = nodes[v[r]];
                ni.x = col_x;
                ni.y = static_cast<double>(r) * (NODE_H + V_SPACING)
                        - col_h / 2.0;
            }
        }
    }

    if (graph.contains("coupling_groups")
        && graph["coupling_groups"].is_array()) {
        for (const auto& group : graph["coupling_groups"]) {
            if (!group.contains("nodes") || !group["nodes"].is_array()) continue;
            double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
            for (const auto& nname : group["nodes"]) {
                auto it = nodeIndex.find(nname.get<std::string>());
                if (it == nodeIndex.end()) continue;
                const auto& ni = nodes[it->second];
                min_x = std::min(min_x, ni.x);
                max_x = std::max(max_x, ni.x + NODE_W);
                min_y = std::min(min_y, ni.y);
                max_y = std::max(max_y, ni.y + NODE_H);
            }
            if (min_x < 1e9) {
                auto* bg = scene_->addRect(
                    min_x - 12, min_y - 12,
                    max_x - min_x + 24, max_y - min_y + 24,
                    QPen(QColor(120, 140, 220, 110), 1, Qt::DashLine),
                    QBrush(QColor(120, 140, 220, 35)));
                bg->setZValue(-2);
            }
        }
    }

    // Edges
    QPen forwardPen(QColor(180, 200, 240), 1.6);
    forwardPen.setCapStyle(Qt::RoundCap);
    QPen backPen(QColor(255, 165, 80), 1.6, Qt::DashLine);
    QBrush forwardArrow(QColor(180, 200, 240));
    QBrush backArrow(QColor(255, 165, 80));

    for (const auto& e : edges) {
        const auto& src = nodes[e.src];
        const auto& tgt = nodes[e.tgt];
        QPointF p1, p2;
        if (!e.back) {
            p1 = QPointF(src.x + NODE_W, src.y + NODE_H * 0.5);
            p2 = QPointF(tgt.x,           tgt.y + NODE_H * 0.5);
        } else {
            p1 = QPointF(src.x,            src.y + NODE_H * 0.5 - 18);
            p2 = QPointF(tgt.x + NODE_W,   tgt.y + NODE_H * 0.5 - 18);
        }
        const double dx = std::max(60.0, std::abs(p2.x() - p1.x()) * 0.5);

        QPainterPath path(p1);
        path.cubicTo(QPointF(p1.x() + dx, p1.y()),
                      QPointF(p2.x() - dx, p2.y()),
                      p2);
        auto* edge = scene_->addPath(path, e.back ? backPen : forwardPen);
        edge->setZValue(-1);
        QString tip = QString("%1.%2  →  %3.%4")
            .arg(QString::fromStdString(src.name),
                  QString::fromStdString(e.field),
                  QString::fromStdString(tgt.name),
                  QString::fromStdString(e.target_field.empty()
                                          ? e.field : e.target_field));
        edge->setToolTip(tip);

        // Arrowhead at p2.
        QPointF tangent = p2 - QPointF(p2.x() - dx * 0.3, p2.y());
        double len = std::hypot(tangent.x(), tangent.y());
        if (len < 1e-6) { tangent = QPointF(1, 0); len = 1.0; }
        tangent /= len;
        const double aSize = 10.0;
        const QPointF base = p2 - tangent * aSize;
        const QPointF perp(-tangent.y(), tangent.x());
        QPolygonF arrow;
        arrow << p2
              << base + perp * (aSize * 0.45)
              << base - perp * (aSize * 0.45);
        scene_->addPolygon(arrow, QPen(Qt::NoPen),
                            e.back ? backArrow : forwardArrow);

        if (!e.field.empty()) {
            auto* lbl = scene_->addText(QString::fromStdString(e.field));
            lbl->setDefaultTextColor(QColor(150, 160, 180));
            QFont f = lbl->font();
            f.setPointSize(7);
            lbl->setFont(f);
            const QPointF mid((p1.x() + p2.x()) * 0.5,
                                (p1.y() + p2.y()) * 0.5 - 10);
            lbl->setPos(mid);
            lbl->setZValue(0);
        }
    }

    // Nodes
    QFont nameFont("Sans", 10, QFont::Bold);
    QFont typeFont("Sans", 8);

    for (auto& ni : nodes) {
        QColor body(56, 60, 78);
        QColor border(120, 140, 200);
        if (ni.type.find("Resistance") != std::string::npos
            || ni.type.find("MLP") != std::string::npos) {
            body = QColor(60, 80, 95);   border = QColor(100, 200, 220);
        } else if (ni.type.find("Field") != std::string::npos
                    || ni.type.find("Magnet") != std::string::npos) {
            body = QColor(80, 60, 90);   border = QColor(220, 140, 220);
        } else if (ni.type.find("RigidBody") != std::string::npos) {
            body = QColor(60, 80, 60);   border = QColor(140, 220, 140);
        } else if (ni.type.find("Lubrication") != std::string::npos
                    || ni.type.find("Friction") != std::string::npos) {
            body = QColor(90, 70, 50);   border = QColor(220, 170, 100);
        } else if (ni.type.find("Gravity") != std::string::npos) {
            body = QColor(70, 70, 70);   border = QColor(180, 180, 180);
        }

        auto* rect = scene_->addRect(
            ni.x, ni.y, NODE_W, NODE_H,
            QPen(border, 2),
            QBrush(body));
        rect->setFlag(QGraphicsItem::ItemIsSelectable);

        // Tooltip: type + each param on its own line.
        QString tip = QString("<b>%1</b><br><i>%2</i>")
            .arg(QString::fromStdString(ni.name),
                  QString::fromStdString(ni.type));
        if (ni.params.is_object() && !ni.params.empty()) {
            tip += "<hr>";
            for (auto& [k, v] : ni.params.items()) {
                tip += QString("%1 = %2<br>")
                    .arg(QString::fromStdString(k),
                          QString::fromStdString(v.dump()));
            }
        }
        rect->setToolTip(tip);

        auto* nameText = scene_->addText(QString::fromStdString(ni.name),
                                          nameFont);
        nameText->setDefaultTextColor(Qt::white);
        nameText->setPos(ni.x + 10, ni.y + 4);
        nameText->setToolTip(tip);

        auto* typeText = scene_->addText(QString::fromStdString(ni.type),
                                          typeFont);
        typeText->setDefaultTextColor(QColor(180, 200, 220));
        typeText->setPos(ni.x + 10, ni.y + 28);
        typeText->setToolTip(tip);
    }
}

} // namespace microbotica::panels
