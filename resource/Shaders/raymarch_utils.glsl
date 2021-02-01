vec2 rayBoxIntersection(vec3 rayOrigin, vec3 rayDirection, 
                        vec3 boxLlf, vec3 boxUrb) 
{
    float t1 = (boxLlf[0] - rayOrigin[0])/rayDirection[0];
    float t2 = (boxUrb[0] - rayOrigin[0])/rayDirection[0];

    float tmin = min(t1, t2);
    float tmax = max(t1, t2);

    for (int i = 1; i < 3; ++i) {
        t1 = (boxLlf[i] - rayOrigin[i])/rayDirection[i];
        t2 = (boxUrb[i] - rayOrigin[i])/rayDirection[i];

        tmin = max(tmin, min(min(t1, t2), tmax));
        tmax = min(tmax, max(max(t1, t2), tmin));
    }

    return vec2(tmin, tmax);
}