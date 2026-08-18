#pragma once

#include <vector>

#include "Luma/Material/MaterialExpression.h"

// The material's node graph: owns the expression nodes. Connections live on
// the nodes' inputs (ExpressionInput), each pointing at a source ExpressionId
// — there is no separate connection list (same model as UE5.8).

namespace Luma::Material {

// UE's FMaterialInputInfo (MaterialGraph.h): the human-readable name +
// property + tooltip that drives the Material Output (root) node's pins.
struct MaterialInputInfo {
    std::string name;           // "Base Color"
    MaterialProperty property;  // MaterialProperty::BaseColor
    std::string tooltip;        // UE FMaterialInputInfo::ToolTip
};

// The ordered table of material property inputs (UE UMaterialGraph::
// MaterialInputs). One entry per MaterialProperty::Count value; the editor's
// root node and the compiler both iterate it.
const std::vector<MaterialInputInfo>& MaterialInputs();

class MaterialGraph {
public:
    MaterialGraph() = default;

    // --- Nodes ---
    // Creates a node of `kind` with a fresh id, appends it, returns its id.
    ExpressionId AddNode(ExpressionKind kind);
    // Removes a node and disconnects every other node's inputs from it.
    void RemoveNode(ExpressionId id);
    void Clear();

    MaterialExpression* Find(ExpressionId id);
    const MaterialExpression* Find(ExpressionId id) const;
    bool Contains(ExpressionId id) const;
    usize Size() const { return m_nodes.size(); }

    std::vector<MaterialExpression>& Nodes() { return m_nodes; }
    const std::vector<MaterialExpression>& Nodes() const { return m_nodes; }

    // Ids are handed out from a counter so new nodes never collide with
    // loaded ones. Persisted so re-saving keeps ids stable.
    ExpressionId NextId() const { return m_nextId; }
    void SetNextId(ExpressionId next) { m_nextId = next; }

    // Walks the graph from `from` and calls `visit` for every reachable
    // expression (including `from`), following input connections. Visits each
    // node at most once.
    void ForEachReachable(ExpressionId from,
                          const std::function<void(MaterialExpression&)>& visit);

private:
    std::vector<MaterialExpression> m_nodes;
    ExpressionId m_nextId = 1;  // 0 is reserved (kInvalidExpressionId)
};

}  // namespace Luma::Material
