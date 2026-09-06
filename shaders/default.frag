#version 330 core

out vec4 FragColor; 

in vec4 v_Color; 
in vec2 v_LocalUV;
flat in vec2 v_BoxSize;
flat in vec4 v_BorderWidths;
flat in vec4 v_BorderColor;
flat in vec4 v_CornerRadii;

// I CANNOT UNDERSTAND THIS MATH FOR THE LIFE OF ME
// THIS IS THE ONLY PART OF MY ENGINE THAT's AI GENERATED
// CAUSE I COULDN'T FIGURE OUT HOW TO DO THIS SHAPE MASKING WITH SDFS

// Inigo Quilez Per-Corner Rounded Box SDF
float sdRoundBox( in vec2 p, in vec2 b, in vec4 r ) 
{
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x  = (p.y > 0.0) ? r.x  : r.y;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

void main()
{
    vec2 b_outer = v_BoxSize * 0.5;
    vec2 p_outer = (v_LocalUV - 0.5) * v_BoxSize;
    
    float dist_outer = sdRoundBox(p_outer, b_outer, v_CornerRadii);

    float aa = length(vec2(dFdx(p_outer.x), dFdy(p_outer.x))); 

    float alpha_outer = 1.0 - smoothstep(-aa, aa, dist_outer);

    float border_T = v_BorderWidths.x;
    float border_R = v_BorderWidths.y;
    float border_B = v_BorderWidths.z;
    float border_L = v_BorderWidths.w;

    vec2 b_inner = b_outer - vec2(border_L + border_R, border_T + border_B) * 0.5;
    vec2 inner_center_offset = vec2((border_L - border_R) * 0.5, (border_B - border_T) * 0.5);
    vec2 p_inner = p_outer - inner_center_offset;

    vec4 r_inner;
    r_inner.x = max(0.0, v_CornerRadii.x - max(border_T, border_R)); 
    r_inner.y = max(0.0, v_CornerRadii.y - max(border_B, border_R)); 
    r_inner.z = max(0.0, v_CornerRadii.z - max(border_T, border_L)); 
    r_inner.w = max(0.0, v_CornerRadii.w - max(border_B, border_L)); 

    float dist_inner = sdRoundBox(p_inner, b_inner, r_inner);
    
    float alpha_inner = 1.0 - smoothstep(-aa, aa, dist_inner);

    vec4 finalColor = mix(v_BorderColor, v_Color, alpha_inner);
    FragColor = finalColor * alpha_outer;
}

