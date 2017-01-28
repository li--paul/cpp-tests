#include <algorithm>
#include <optional>

#include "CollisionSolver.h"
#include "Shape.h"

using namespace std;
using namespace math;

namespace
{
	struct Projection
	{
		float min;
		float max;
	};

	struct Overlap
	{
		float depth;
		Vec2 direction;
	};

	bool areBothStatic(float const massInverseA, float const massInverseB)
	{
		return massInverseA == 0 && massInverseB == 0;
	}

	bool boundsOverlap(Bounds const & boundsA, Bounds const & boundsB)
	{
		if (boundsA.bottomRight.x <= boundsB.topLeft.x)
			return false;

		if (boundsA.topLeft.x >= boundsB.bottomRight.x)
			return false;

		if (boundsA.bottomRight.y <= boundsB.topLeft.y)
			return false;

		if (boundsA.topLeft.y >= boundsB.bottomRight.y)
			return false;

		return true;
	}

	Projection project(vector<Vec2> const & vertices, Vec2 const n)
	{
		float min{ numeric_limits<float>::max() };
		float max{ numeric_limits<float>::lowest() };

		// project each vertex on normal and store minimum and maximum projection
		for(Vec2 const & v : vertices)
		{
			float proj{ n.dot(v) };

			if (proj < min)
				min = proj;

			if (proj > max)
				max = proj;
		}

		return{ min, max };
	}

	optional<Overlap> checkOverlap(Vec2 const posA, Vec2 const posB, vector<Vec2> const & verticesA, vector<Vec2> const & verticesB)
	{
		Vec2 const d{ posB - posA };
		float minPenetration{ numeric_limits<float>::max() };
		Vec2 normal;

		// for each edge of the shape A get it's normal and project both shapes on that normal
		for (size_t v1{ verticesA.size() - 1 }, v2{ 0 }; v2 < verticesA.size(); v1 = v2, ++v2)
		{
			Vec2 const vertex1{ verticesA[v1] };
			Vec2 const vertex2{ verticesA[v2] };

			Vec2 const edge{ vertex2 - vertex1 };
			Vec2 const n{ edge.getNormal() };

			Projection projA{ project(verticesA, n) };
			Projection projB{ project(verticesB, n) };
			float const projD{ n.dot(d) };

			projB.min += projD;
			projB.max += projD;

			// if projections on the same normal do not overlap that means that shapes are separated, i.e. no overlap 
			if (projB.min >= projA.max || projB.max <= projA.min)
				return {};

			// get the overlap size and store the minimum one as well as a normal for this minimum overlap.
			// later move shapes along this normal by the value of the minimum overlap
			float const p{ abs(projA.max - projB.min) };
			if (p < minPenetration)
			{
				minPenetration = p;
				normal = n;
			}
		}

		return Overlap{ minPenetration, normal };
	}
}

void CollisionSolver::solveCollision(float const massInverseA, float const massInverseB, Bounds const & boundsA, Bounds const & boundsB, Vec2 const posA, Vec2 const posB, vector<Vec2> const & verticesA, vector<Vec2> const & verticesB, Vec2 & velocityA, Vec2 & velocityB, Vec2 & overlapAccumulatorA, Vec2 & overlapAccumulatorB)
{
	if (areBothStatic(massInverseA, massInverseB) || !boundsOverlap(boundsA, boundsB))
		return;

	optional<Overlap> const result1(checkOverlap(posA, posB, verticesA, verticesB));
	if (!result1.has_value())
		return;

	optional<Overlap> const result2(checkOverlap(posB, posA, verticesB, verticesA));
	if (!result2.has_value())
		return;

	Overlap const & overlap1{ result1.value() };
	Overlap const & overlap2{ result2.value() };

	float const d{ min(overlap1.depth, overlap2.depth) };
	Vec2 const n{ (overlap1.depth < overlap2.depth) ? overlap1.direction : overlap2.direction };

	float d1{ d * massInverseA / (massInverseA + massInverseB) };
	float d2{ d - d1 };

	if (overlap1.depth >= overlap2.depth)
	{
		d1 *= -1;
		d2 *= -1;
	}

	Vec2 const vRel{ velocityA - velocityB };

	float const j{ -n.dot(vRel) * 2 / (massInverseA + massInverseB) };

	velocityA += n * (j * massInverseA);
	velocityB -= n * (j * massInverseB);

	// don't move shapes apart immediately but instead accumulate overlap resolution and apply it later in one go
	overlapAccumulatorA -= n * d1;
	overlapAccumulatorB += n * d2;
}