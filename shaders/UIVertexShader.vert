#version 460
#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) out vec4 backGroundColor;

struct UIContainer
{
	uvec2 bitfields;  // x - bitfields y - children count
	vec2 relativeContainerSize; // percent size of the canvas
	vec2 absoluteSize;
    vec2 anchorPoint;
	vec4 color;
};

struct WindowSize
{
    float width;
    float height;
    uint totalUIContainerCount;
};

layout(push_constant) uniform GlobalContext {
    WindowSize window;
} gs;

layout(set = 0, binding = 0) readonly buffer UIContainers
{
    UIContainer renderables[];
} conts;

void main()
{
    uint renderableIndex = uint(gl_DrawID);

    UIContainer container = conts.renderables[renderableIndex];

    vec2 poses[4] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 1.0)
    );

    float startX = container.anchorPoint.x/gs.window.width;
    float startY = container.anchorPoint.y/gs.window.height;

    float endX = container.absoluteSize.x/gs.window.width;
    float endY = container.absoluteSize.y/gs.window.height;

    vec2 mixer = poses[gl_VertexIndex];

    vec2 pos =  2.0 * vec2(((endX - startX) * mixer.x) + startX,  ((endY - startY) * mixer.y) + startY) - 1.0;

    gl_Position = vec4(pos, 0.0, 1.0);

    backGroundColor = container.color;
}