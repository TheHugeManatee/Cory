module Raycast;

public static const float kEpsilon = 1e-6;
public static const float kInfinity = 1e20;
public static const uint kMaxIntervals = 8;
public static const uint kMaxWorkIntervals = kMaxIntervals * 2;

public enum PrimitiveKind : uint
{
    None = 0,
    AABB = 1,
    OBB = 2,
    Sphere = 3,
    CSG = 4,
}

public enum CsgOp : uint
{
    Union = 0,
    Intersect = 1,
    Subtract = 2,
}

public struct Ray
{
    public float3 origin;
    public float3 direction;
    public float tMin;
    public float tMax;

    public __init(float3 origin, float3 direction, float tMin = 0.0, float tMax = kInfinity)
    {
        this.origin = origin;
        this.direction = direction;
        this.tMin = tMin;
        this.tMax = tMax;
    }

    public float3 at(float t)
    {
        return origin + direction * t;
    }
}

public struct HitInfo
{
    public bool hit;
    public float tEnter;
    public float tExit;
    public float3 normal;
    public PrimitiveKind primitiveKind;
    public uint primitiveIndex;

    public __init(
        bool hit,
        float tEnter,
        float tExit,
        float3 normal,
        PrimitiveKind primitiveKind = PrimitiveKind::None,
        uint primitiveIndex = 0u)
    {
        this.hit = hit;
        this.tEnter = tEnter;
        this.tExit = tExit;
        this.normal = normal;
        this.primitiveKind = primitiveKind;
        this.primitiveIndex = primitiveIndex;
    }
}

public struct Interval
{
    public float t0;
    public float t1;
    public float3 n0;
    public float3 n1;
    public PrimitiveKind kind;
    public uint primitiveIndex;

    public __init(
        float t0,
        float t1,
        float3 n0,
        float3 n1,
        PrimitiveKind kind = PrimitiveKind::None,
        uint primitiveIndex = 0u)
    {
        this.t0 = t0;
        this.t1 = t1;
        this.n0 = n0;
        this.n1 = n1;
        this.kind = kind;
        this.primitiveIndex = primitiveIndex;
    }
}

public struct IntervalSet
{
    public uint count;
    public Interval intervals[kMaxIntervals];

    public __init()
    {
        count = 0u;
        for (uint i = 0u; i < kMaxIntervals; ++i)
            intervals[i] = Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0));
    }
}

public interface IRayIntersectable
{
    IntervalSet intervals(Ray ray);
}

public struct AABB : IRayIntersectable
{
    public float3 min;
    public float3 max;

    public __init(float3 min, float3 max)
    {
        this.min = min;
        this.max = max;
    }

    public IntervalSet intervals(Ray ray)
    {
        return intervalsFromHit(intersect(ray, this));
    }
}

public struct OBB : IRayIntersectable
{
    public float3 center;
    public float3 axisX;
    public float3 axisY;
    public float3 axisZ;
    public float3 halfExtent;

    public __init(float3 center, float3 axisX, float3 axisY, float3 axisZ, float3 halfExtent)
    {
        this.center = center;
        this.axisX = axisX;
        this.axisY = axisY;
        this.axisZ = axisZ;
        this.halfExtent = halfExtent;
    }

    public IntervalSet intervals(Ray ray)
    {
        return intervalsFromHit(intersect(ray, this));
    }
}

public struct Sphere : IRayIntersectable
{
    public float3 center;
    public float radius;

    public __init(float3 center, float radius)
    {
        this.center = center;
        this.radius = radius;
    }

    public IntervalSet intervals(Ray ray)
    {
        return intervalsFromHit(intersect(ray, this));
    }
}

HitInfo missHit()
{
    return HitInfo(false, 0.0, 0.0, float3(0.0, 0.0, 0.0), PrimitiveKind::None, 0u);
}

IntervalSet emptyIntervals()
{
    return IntervalSet();
}

bool appendInterval(inout IntervalSet set, Interval interval)
{
    if (set.count >= kMaxIntervals)
        return false;

    set.intervals[set.count] = interval;
    set.count++;
    return true;
}

bool overlapsOrTouches(Interval a, Interval b)
{
    return b.t0 <= (a.t1 + kEpsilon);
}

void sortIntervalsByEntry(inout Interval values[kMaxWorkIntervals], uint count)
{
    for (uint i = 1u; i < count; ++i) {
        let current = values[i];
        var j = int(i) - 1;

        while (j >= 0 && values[uint(j)].t0 > current.t0) {
            values[uint(j + 1)] = values[uint(j)];
            --j;
        }

        values[uint(j + 1)] = current;
    }
}

float3 axisNormal(uint axis, float sign)
{
    if (axis == 0u)
        return float3(sign, 0.0, 0.0);
    if (axis == 1u)
        return float3(0.0, sign, 0.0);
    return float3(0.0, 0.0, sign);
}

public bool isValidHit(HitInfo hit)
{
    return hit.hit && (hit.tEnter <= hit.tExit) && (hit.tExit >= 0.0);
}

public IntervalSet intervalsFromHit(HitInfo hit)
{
    let set = emptyIntervals();
    if (!isValidHit(hit))
        return set;

    var result = set;
    appendInterval(result, Interval(hit.tEnter, hit.tExit, hit.normal, hit.normal, hit.primitiveKind, hit.primitiveIndex));
    return result;
}

public HitInfo closestHit(IntervalSet set, Ray ray)
{
    var best = missHit();
    var bestT = ray.tMax;

    for (uint i = 0u; i < set.count; ++i) {
        let interval = set.intervals[i];
        let enter = max(interval.t0, ray.tMin);
        let exit = min(interval.t1, ray.tMax);

        if (enter > exit)
            continue;

        if (enter < bestT) {
            bestT = enter;
            best = HitInfo(true, enter, exit, interval.n0, interval.kind, interval.primitiveIndex);
        }
    }

    return best;
}

public HitInfo mergeCloser(HitInfo a, HitInfo b)
{
    if (!isValidHit(a))
        return b;
    if (!isValidHit(b))
        return a;
    return (a.tEnter <= b.tEnter) ? a : b;
}

public HitInfo intersect(Ray ray, AABB box)
{
    var tNear = ray.tMin;
    var tFar = ray.tMax;

    var enterNormal = float3(0.0, 0.0, 0.0);
    var haveEnterNormal = false;

    for (uint axis = 0u; axis < 3u; ++axis) {
        let originValue = ray.origin[axis];
        let dirValue = ray.direction[axis];
        let minBound = box.min[axis];
        let maxBound = box.max[axis];

        if (abs(dirValue) < kEpsilon) {
            if (originValue < minBound || originValue > maxBound)
                return missHit();
            continue;
        }

        let invDir = 1.0 / dirValue;
        var t0 = (minBound - originValue) * invDir;
        var t1 = (maxBound - originValue) * invDir;
        var n0 = axisNormal(axis, -1.0);
        var n1 = axisNormal(axis, 1.0);

        if (t0 > t1) {
            let tmpT = t0;
            t0 = t1;
            t1 = tmpT;

            let tmpN = n0;
            n0 = n1;
            n1 = tmpN;
        }

        if (t0 > tNear) {
            tNear = t0;
            enterNormal = n0;
            haveEnterNormal = true;
        }

        tFar = min(tFar, t1);
        if (tNear > tFar)
            return missHit();
    }

    if (!haveEnterNormal)
        enterNormal = -normalize(ray.direction);

    return HitInfo(true, tNear, tFar, enterNormal, PrimitiveKind::AABB, 0u);
}

public HitInfo intersect(Ray ray, Sphere sphere)
{
    let oc = ray.origin - sphere.center;
    let a = dot(ray.direction, ray.direction);
    let b = 2.0 * dot(oc, ray.direction);
    let c = dot(oc, oc) - sphere.radius * sphere.radius;

    let discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0)
        return missHit();

    let sqrtDiscriminant = sqrt(discriminant);
    var t0 = (-b - sqrtDiscriminant) / (2.0 * a);
    var t1 = (-b + sqrtDiscriminant) / (2.0 * a);
    if (t0 > t1) {
        let tmp = t0;
        t0 = t1;
        t1 = tmp;
    }

    let tEnter = max(t0, ray.tMin);
    let tExit = min(t1, ray.tMax);
    if (tEnter > tExit)
        return missHit();

    var enterNormal = normalize(ray.at(tEnter) - sphere.center);
    if (t0 < ray.tMin)
        enterNormal = -normalize(ray.direction);

    return HitInfo(true, tEnter, tExit, enterNormal, PrimitiveKind::Sphere, 0u);
}

public HitInfo intersect(Ray ray, OBB obb)
{
    var tNear = ray.tMin;
    var tFar = ray.tMax;

    let axis0 = normalize(obb.axisX);
    let axis1 = normalize(obb.axisY);
    let axis2 = normalize(obb.axisZ);
    let axes = float3[3](axis0, axis1, axis2);

    let relOrigin = ray.origin - obb.center;

    var enterNormal = float3(0.0, 0.0, 0.0);
    var haveEnterNormal = false;

    for (uint axisIndex = 0u; axisIndex < 3u; ++axisIndex) {
        let axis = axes[axisIndex];
        let extent = obb.halfExtent[axisIndex];

        let localOrigin = dot(relOrigin, axis);
        let localDir = dot(ray.direction, axis);

        if (abs(localDir) < kEpsilon) {
            if (localOrigin < -extent || localOrigin > extent)
                return missHit();
            continue;
        }

        var t0 = (-extent - localOrigin) / localDir;
        var t1 = (extent - localOrigin) / localDir;
        var n0 = -axis;
        var n1 = axis;

        if (t0 > t1) {
            let tmpT = t0;
            t0 = t1;
            t1 = tmpT;

            let tmpN = n0;
            n0 = n1;
            n1 = tmpN;
        }

        if (t0 > tNear) {
            tNear = t0;
            enterNormal = n0;
            haveEnterNormal = true;
        }

        tFar = min(tFar, t1);
        if (tNear > tFar)
            return missHit();
    }

    if (!haveEnterNormal)
        enterNormal = -normalize(ray.direction);

    return HitInfo(true, tNear, tFar, enterNormal, PrimitiveKind::OBB, 0u);
}

public IntervalSet csgUnion(IntervalSet a, IntervalSet b)
{
    var merged = emptyIntervals();

    var all = Interval[kMaxWorkIntervals](
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)),
        Interval(0.0, 0.0, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0)));

    var allCount = 0u;

    for (uint i = 0u; i < a.count && allCount < kMaxWorkIntervals; ++i)
        all[allCount++] = a.intervals[i];

    for (uint i = 0u; i < b.count && allCount < kMaxWorkIntervals; ++i)
        all[allCount++] = b.intervals[i];

    if (allCount == 0u)
        return merged;

    sortIntervalsByEntry(all, allCount);

    var current = all[0u];
    current.kind = PrimitiveKind::CSG;

    for (uint i = 1u; i < allCount; ++i) {
        let next = all[i];

        if (overlapsOrTouches(current, next)) {
            if (next.t1 > current.t1) {
                current.t1 = next.t1;
                current.n1 = next.n1;
            }
            current.kind = PrimitiveKind::CSG;
        } else {
            appendInterval(merged, current);
            current = next;
            current.kind = PrimitiveKind::CSG;
        }
    }

    appendInterval(merged, current);
    return merged;
}

public IntervalSet csgIntersect(IntervalSet a, IntervalSet b)
{
    var result = emptyIntervals();

    var i = 0u;
    var j = 0u;

    while (i < a.count && j < b.count) {
        let ai = a.intervals[i];
        let bj = b.intervals[j];

        let t0 = max(ai.t0, bj.t0);
        let t1 = min(ai.t1, bj.t1);

        if (t0 <= t1) {
            let n0 = (ai.t0 >= bj.t0) ? ai.n0 : bj.n0;
            let n1 = (ai.t1 <= bj.t1) ? ai.n1 : bj.n1;
            appendInterval(result, Interval(t0, t1, n0, n1, PrimitiveKind::CSG, 0u));
        }

        if (ai.t1 < bj.t1)
            ++i;
        else
            ++j;
    }

    return result;
}

public IntervalSet csgSubtract(IntervalSet a, IntervalSet b)
{
    var result = emptyIntervals();

    for (uint ai = 0u; ai < a.count; ++ai) {
        var pieces = emptyIntervals();
        appendInterval(pieces, a.intervals[ai]);

        for (uint bi = 0u; bi < b.count; ++bi) {
            let cut = b.intervals[bi];
            var nextPieces = emptyIntervals();

            for (uint pi = 0u; pi < pieces.count; ++pi) {
                let piece = pieces.intervals[pi];

                if (cut.t1 <= piece.t0 || cut.t0 >= piece.t1) {
                    appendInterval(nextPieces, piece);
                    continue;
                }

                if (cut.t0 > piece.t0) {
                    let left = Interval(
                        piece.t0,
                        min(cut.t0, piece.t1),
                        piece.n0,
                        -cut.n0,
                        PrimitiveKind::CSG,
                        0u);

                    if (left.t0 <= left.t1)
                        appendInterval(nextPieces, left);
                }

                if (cut.t1 < piece.t1) {
                    let right = Interval(
                        max(cut.t1, piece.t0),
                        piece.t1,
                        -cut.n1,
                        piece.n1,
                        PrimitiveKind::CSG,
                        0u);

                    if (right.t0 <= right.t1)
                        appendInterval(nextPieces, right);
                }
            }

            pieces = nextPieces;
            if (pieces.count == 0u)
                break;
        }

        for (uint pi = 0u; pi < pieces.count; ++pi)
            appendInterval(result, pieces.intervals[pi]);
    }

    return result;
}

public IntervalSet csgApply(IntervalSet a, IntervalSet b, CsgOp op)
{
    switch (op) {
    case CsgOp::Union:
        return csgUnion(a, b);
    case CsgOp::Intersect:
        return csgIntersect(a, b);
    default:
        return csgSubtract(a, b);
    }
}

public IntervalSet evalIntervals<T : IRayIntersectable>(T shape, Ray ray)
{
    return shape.intervals(ray);
}

public HitInfo evalClosest<T : IRayIntersectable>(T shape, Ray ray)
{
    return closestHit(shape.intervals(ray), ray);
}

public IntervalSet combine<A : IRayIntersectable, B : IRayIntersectable>(A a, B b, Ray ray, CsgOp op)
{
    return csgApply(evalIntervals(a, ray), evalIntervals(b, ray), op);
}
