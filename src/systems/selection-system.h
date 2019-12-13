#pragma once

#include "i-system.h"
#include "context.h"

#include "scomponents/io/selection.h"

class SelectionSystem : public ISystem {
public:
    SelectionSystem(Context& ctx, SingletonComponents& scomps);
    virtual ~SelectionSystem();

	void update() override;

private:
    scomp::Face colorToFace(unsigned char color) const;

private:
    Context& m_ctx;
    SingletonComponents& m_scomps;
};
