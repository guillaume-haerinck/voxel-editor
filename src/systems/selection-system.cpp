#include "selection-system.h"

#include <spdlog/spdlog.h>
#include <debug_break/debug_break.h>
#include "maths/intersection.h"
#include "maths/casting.h"
#include "components/physics/transform.h"

// Temp
#ifdef __EMSCRIPTEN__
	#include <GLES3/gl3.h>
#else
	#include <glad/gles2.h>
#endif
#include "graphics/gl-exception.h"

SelectionSystem::SelectionSystem(Context& ctx, SingletonComponents& scomps) 
    : m_ctx(ctx), m_scomps(scomps)
{
    // TODO set 10 to max cube height or width
    m_planePositions = {
        glm::vec3(0),
        //glm::vec3(0),
        //glm::vec3(0, 10, 0),
        //glm::vec3(0),
        //glm::vec3(0),
        //glm::vec3(0)
    };

    m_planeNormals = {
        glm::vec3(-1, 0, 0),      // right
        //glm::vec3(1, 0, 0),     // 
        //glm::vec3(0, 1, 0),     // 
        //glm::vec3(-1, 0, 0),    // 
        //glm::vec3(0,-1, 0),     // 
        //glm::vec3(0, 0,-1)      // 
    };

}

SelectionSystem::~SelectionSystem() {}

void SelectionSystem::update() {
    m_ctx.rcommand.bindRenderTargets(m_scomps.renderTargets.at(scomp::RenderTargetsIndex::RTT_GEOMETRY));

    // TODO abstract
    unsigned char pixel[] = { 0, 0, 0, 0 };
    GLCall(glReadBuffer(GL_COLOR_ATTACHMENT3));
    GLCall(glReadPixels(m_scomps.inputs.mousePos.x, 500 - m_scomps.inputs.mousePos.y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel));

    m_scomps.hovered.exist = false;
    const met::entity hoveredCube = voxmt::colorToInt(pixel[0], pixel[1], pixel[2]);
    
    // Check existing cubes with framebuffer
    if (hoveredCube != met::null_entity) {
        m_scomps.hovered.exist = true;
        m_scomps.hovered.face = colorToFace(pixel[3]);
        const comp::Transform trans = m_ctx.registry.get<comp::Transform>(hoveredCube);
        m_scomps.hovered.position = trans.position;
        
    } else { 
        // Check grid with raycast
        const glm::mat4 toWorld = glm::inverse((m_scomps.camera.proj * m_scomps.camera.view));
        glm::vec4 from = toWorld * glm::vec4(m_scomps.inputs.NDCMousePos, -1.0f, 1.0f);
        glm::vec4 to = toWorld * glm::vec4(m_scomps.inputs.NDCMousePos, 1.0f, 1.0f);
        from /= from.w;
        to /= to.w;

        glm::vec3 intersectionPoint;
        for (unsigned int i = 0; i < m_planeNormals.size(); i++) {
            if (voxmt::doesLineIntersectPlane(m_planeNormals.at(i), m_planePositions.at(i), from, to, intersectionPoint)) {
                if (intersectionPoint.x >= 0.0f && intersectionPoint.y >= 0.0f && intersectionPoint.z >= 0.0f) {
                    m_scomps.hovered.exist = true;
                    m_scomps.hovered.face = normalToFace(m_planeNormals.at(i));
                    m_scomps.hovered.position = voxmt::roundUpFloat3(intersectionPoint);

                    auto test = voxmt::roundUpFloat3(intersectionPoint);
                    spdlog::info("Intersection is at : {} {} {}", test.x, test.y, test.z);
                    break;
                }
            }
        }
    }
}

scomp::Face SelectionSystem::colorToFace(unsigned char color) const {
    switch (color) {
        case 0: return scomp::Face::NONE;
        case 1: return scomp::Face::BACK;
        case 2: return scomp::Face::RIGHT;
        case 3: return scomp::Face::TOP;
        case 4: return scomp::Face::FRONT;
        case 5: return scomp::Face::LEFT;
        case 6: return scomp::Face::BOTTOM;
        default:
            debug_break();
            assert(false && "Unknown face picked");
            return scomp::Face::NONE;
    }
}

scomp::Face SelectionSystem::normalToFace(const glm::vec3& normal) const {
    return scomp::Face::FRONT;
}
