#version 330 core

out vec4 FragColor; 

in vec4 v_Color; 
in vec2 v_PosRelToCenter;
in vec2 v_TexCoord;
flat in uint v_ElementType;
flat in float v_TextureLayer;
flat in vec2 v_BoxSize;
flat in vec4 v_BorderWidths;
flat in vec4 v_BorderColor;
flat in vec4 v_CornerRadii;
flat in float v_ShadowBlur;
flat in vec2  v_ShadowOffset;
flat in vec4  v_ShadowColor;

const int MAX_CLIP_DEPTH = 16;
uniform vec2 u_resolution;
uniform int u_clipCount;
uniform vec4 u_clipRects[MAX_CLIP_DEPTH];
uniform vec4 u_clipRadii[MAX_CLIP_DEPTH];
uniform sampler2DArray u_glyphAtlas;

const float EPSILON = 1e-5;

float sdRoundBox( in vec2 p, in vec2 b, in vec4 r ) 
{
    float rad = (p.x < 0.0) ? ((p.y < 0.0) ? r.x : r.z)
                            : ((p.y < 0.0) ? r.y : r.w);
    vec2 q = abs(p) - b + rad;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - rad;
}

float shadowAlpha(vec2 p, vec2 b, vec4 r, float blur) {
    if (blur <= 0.0) {
        return 1.0 - step(0.0, sdRoundBox(p, b, r));
    }
    float dist = sdRoundBox(p, b, r);
    return 1.0 - smoothstep(-blur, blur, dist);
}

vec2 computeInnerBoxSize(vec2 bOuter, vec4 borderWidths)
{
    return bOuter - vec2(borderWidths.w + borderWidths.y, borderWidths.x + borderWidths.z) * 0.5;
}

vec2 computeInnerCenterOffset(vec4 borderWidths)
{
    return vec2((borderWidths.w - borderWidths.y) * 0.5, (borderWidths.x - borderWidths.z) * 0.5);
}

vec4 computeInnerCornerRadii(vec4 cornerRadii, vec4 borderWidths)
{
    vec4 innerRadii;

    innerRadii.x = max(0.0, cornerRadii.x - max(borderWidths.x, borderWidths.w));
    innerRadii.y = max(0.0, cornerRadii.y - max(borderWidths.x, borderWidths.y));
    innerRadii.z = max(0.0, cornerRadii.z - max(borderWidths.z, borderWidths.w));
    innerRadii.w = max(0.0, cornerRadii.w - max(borderWidths.z, borderWidths.y));

    return innerRadii;
}

float computeAlphaFromDistance(float dist, float aa)
{
    return 1.0 - smoothstep(-aa, aa, dist);
}

vec4 buildShadowLayer(vec2 pOuter, vec2 bOuter, vec4 cornerRadii, float shadowBlur, vec2 shadowOffset, vec4 shadowColor)
{
    vec2 pShadow = pOuter - shadowOffset;
    float alphaShadow = shadowAlpha(pShadow, bOuter, cornerRadii, shadowBlur);
    return vec4(shadowColor.rgb, shadowColor.a * alphaShadow);
}

float computeClipAlpha()
{
    float clipAlpha = 1.0;
    vec2 fragmentPosition = vec2(gl_FragCoord.x, u_resolution.y - gl_FragCoord.y);
    for (int i = 0; i < MAX_CLIP_DEPTH; ++i)
    {
        if (i >= u_clipCount)
            break;

        vec4 rect = u_clipRects[i];
        vec2 halfSize = rect.zw * 0.5;
        vec2 relativePosition = fragmentPosition - rect.xy - halfSize;
        float clipDistance = sdRoundBox(relativePosition, halfSize, u_clipRadii[i]);
        float clipAA = max(fwidth(relativePosition.x), fwidth(relativePosition.y));
        clipAlpha *= computeAlphaFromDistance(clipDistance, clipAA);
    }
    return clipAlpha;
}

void main()
{
    if (v_ElementType == 2u)
    {
        float coverage = texture(
            u_glyphAtlas, vec3(v_TexCoord, v_TextureLayer)).r;
        FragColor = vec4(v_Color.rgb,
                         v_Color.a * coverage * computeClipAlpha());
        return;
    }

    vec2 b_outer = v_BoxSize * 0.5;
    vec2 p_outer = v_PosRelToCenter;

    float aa = max(fwidth(p_outer.x), fwidth(p_outer.y));

    bool hasCornerRadii = any(greaterThan(abs(v_CornerRadii), vec4(EPSILON)));
    bool hasBorder = any(greaterThan(abs(v_BorderWidths), vec4(EPSILON)));
    bool hasShadow = (v_ShadowBlur > EPSILON) || (length(v_ShadowOffset) > EPSILON) || (abs(v_ShadowColor.a) > EPSILON);

    // 1. SHADOW PASS
    vec4 shadowLayer = vec4(0.0);
    if (hasShadow)
    {
        shadowLayer = buildShadowLayer(p_outer, b_outer, v_CornerRadii, v_ShadowBlur, v_ShadowOffset, v_ShadowColor);
    }

    // 2. OUTER BOUNDARY PASS
    vec4 outerCornerRadii = hasCornerRadii ? v_CornerRadii : vec4(0.0);
    float dist_outer = sdRoundBox(p_outer, b_outer, outerCornerRadii);
    float alpha_outer = computeAlphaFromDistance(dist_outer, aa);

    // 3. INWARD AREA SHRINKING BORDER PASS
    vec2 b_inner = b_outer;
    vec2 p_inner = p_outer;
    vec4 r_inner = vec4(0.0);
    float alpha_inner = 1.0;

    if (hasBorder)
    {
        b_inner = computeInnerBoxSize(b_outer, v_BorderWidths);
        p_inner = p_outer - computeInnerCenterOffset(v_BorderWidths);
        r_inner = computeInnerCornerRadii(v_CornerRadii, v_BorderWidths);

        float dist_inner = sdRoundBox(p_inner, b_inner, r_inner);
        alpha_inner = computeAlphaFromDistance(dist_inner, aa);
    }

    // 4. COMPOSITION PASS
    vec4 elementColor = mix(v_BorderColor, v_Color, alpha_inner);
    elementColor.a *= alpha_outer;

    vec4 finalColor = mix(shadowLayer, elementColor, elementColor.a);

    finalColor.a *= computeClipAlpha();
    FragColor = finalColor;
}