#version 330 core

out vec4 FragColor; 

in vec4 v_Color; 
in vec2 v_PosRelToCenter;
flat in vec2 v_BoxSize;
flat in vec4 v_BorderWidths;
flat in vec4 v_BorderColor;
flat in vec4 v_CornerRadii;
flat in float v_ShadowBlur;
flat in vec2  v_ShadowOffset;
flat in vec4  v_ShadowColor;

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

void main()
{
    vec2 b_outer = v_BoxSize * 0.5;
    vec2 p_outer = v_PosRelToCenter;
    
    float aa = length(vec2(dFdx(p_outer.x), dFdy(p_outer.x))); 

    // 1. SHADOW PASS
    vec2 p_shadow = p_outer - v_ShadowOffset;
    float alpha_shadow = shadowAlpha(p_shadow, b_outer, v_CornerRadii, v_ShadowBlur);
    vec4 shadowLayer = vec4(v_ShadowColor.rgb, v_ShadowColor.a * alpha_shadow);

    // 2. OUTER BOUNDARY PASS
    float dist_outer = sdRoundBox(p_outer, b_outer, v_CornerRadii);
    float alpha_outer = 1.0 - smoothstep(-aa, aa, dist_outer);

    // 3. INWARD AREA SHRINKING BORDER PASS
    float border_T = v_BorderWidths.x;
    float border_R = v_BorderWidths.y;
    float border_B = v_BorderWidths.z;
    float border_L = v_BorderWidths.w;

    vec2 b_inner = b_outer - vec2(border_L + border_R, border_T + border_B) * 0.5;
    vec2 inner_center_offset = vec2((border_L - border_R) * 0.5, (border_T - border_B) * 0.5);
    vec2 p_inner = p_outer - inner_center_offset;

    vec4 r_inner;
    r_inner.x = max(0.0, v_CornerRadii.x - max(border_T, border_L));
    r_inner.y = max(0.0, v_CornerRadii.y - max(border_T, border_R));
    r_inner.z = max(0.0, v_CornerRadii.z - max(border_B, border_L));
    r_inner.w = max(0.0, v_CornerRadii.w - max(border_B, border_R));

    float dist_inner = sdRoundBox(p_inner, b_inner, r_inner);
    float alpha_inner = 1.0 - smoothstep(-aa, aa, dist_inner);

    // 4. COMPOSITION PASS
    vec4 elementColor = mix(v_BorderColor, v_Color, alpha_inner);
    elementColor.a *= alpha_outer;

    vec4 finalColor = mix(shadowLayer, elementColor, elementColor.a);
    FragColor = finalColor;
}