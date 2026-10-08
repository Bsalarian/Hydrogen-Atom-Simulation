#include "color/heatmap.h"

glm::vec3 heatmapInferno(float t) {

    struct Stop { float p; glm::vec3 c; };
    static const Stop stops[] = {
        {0.00f, {0.00f, 0.00f, 0.00f}},
        {0.15f, {0.15f, 0.00f, 0.30f}},
        {0.35f, {0.50f, 0.00f, 0.60f}},
        {0.55f, {0.90f, 0.10f, 0.20f}},
        {0.75f, {1.00f, 0.50f, 0.00f}},
        {0.90f, {1.00f, 0.90f, 0.10f}},
        {1.00f, {1.00f, 1.00f, 1.00f}},
    };

    int colorCount = sizeof(stops) / sizeof(stops[0]);

    t = glm::clamp(t, 0.0f, 1.0f);
    
    for (int i = 0; i < colorCount - 1; ++i) {
        if (t >= stops[i+1].p) {
            float local = (t - stops[i].p) / (stops[i + 1].p - stops[i].p);
            return glm::mix(stops[i].c, stops[i + 1].c, local);
        }
    }
    return stops[colorCount - 1].c;
}