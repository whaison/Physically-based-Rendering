/**
 * Check all faces of a leaf node for intersections with the given ray.
 * @param {const int}             nodeIndex
 * @param {const int}             faceIndex
 * @param {const float4*}         origin
 * @param {const float4*}         dir
 * @param {const global float*}   scVertices
 * @param {const global uint*}    scFaces
 * @param {const global int*}     kdNodeFaces
 * @param {const float}           entryDistance
 * @param {float*}                exitDistance
 * @param {hit_t*}                result
 */
void checkFaces(
	ray4* ray, const int nodeIndex, const int faceIndex,
	const int numFaces, const global uint* kdFaces,
	const global float4* scVertices, const global uint4* scFaces,
	const float entryDistance, float* exitDistance, const float boxExitLimit
) {
	float4 a, b, c;
	float4 prefetch_a, prefetch_b, prefetch_c;
	float r;
	uint j = kdFaces[faceIndex];

	prefetch_a = scVertices[scFaces[j].x];
	prefetch_b = scVertices[scFaces[j].y];
	prefetch_c = scVertices[scFaces[j].z];

	for( uint i = 1; i <= numFaces; i++ ) {
		a = prefetch_a;
		b = prefetch_b;
		c = prefetch_c;
		j = kdFaces[faceIndex + i];

		prefetch_a = scVertices[scFaces[j].x];
		prefetch_b = scVertices[scFaces[j].y];
		prefetch_c = scVertices[scFaces[j].z];

		r = checkFaceIntersection(
			ray->origin, ray->dir, a, b, c,
			entryDistance, fmin( *exitDistance, boxExitLimit )
		);

		if( r > -1.0f ) {
			*exitDistance = r;

			if( ray->t > r || ray->nodeIndex < 0 ) {
				ray->t = r;
				ray->nodeIndex = nodeIndex;
				ray->faceIndex = kdFaces[faceIndex + i - 1];
			}
		}
	}
}


/**
 * Check all faces of a leaf node for intersections with the given ray to test if it can reach a light source.
 * If a hit is found between origin and light source, the function returns immediately.
 * @param  {const int}           nodeIndex
 * @param  {const int}           faceIndex
 * @param  {const float4*}       origin
 * @param  {const float4*}       dir
 * @param  {const global float*} scVertices
 * @param  {const global uint*}  scFaces
 * @param  {const global int*}   kdNodeFaces
 * @param  {const float}         entryDistance
 * @param  {float*}              exitDistance
 * @return {bool}                              True, if a hit is detected between origin and light, false otherwise.
 */
bool checkFacesForShadow(
	ray4* ray, const int nodeIndex, const int faceIndex,
	const int numFaces, const global uint* kdFaces,
	const global float4* scVertices, const global uint4* scFaces,
	const float entryDistance, float* exitDistance
) {
	float4 a, b, c;
	float4 prefetch_a, prefetch_b, prefetch_c;
	float r;
	uint j = kdFaces[faceIndex];

	prefetch_a = scVertices[scFaces[j].x];
	prefetch_b = scVertices[scFaces[j].y];
	prefetch_c = scVertices[scFaces[j].z];

	for( uint i = 1; i <= numFaces; i++ ) {
		a = prefetch_a;
		b = prefetch_b;
		c = prefetch_c;
		j = kdFaces[faceIndex + i];

		prefetch_a = scVertices[scFaces[j].x];
		prefetch_b = scVertices[scFaces[j].y];
		prefetch_c = scVertices[scFaces[j].z];

		r = checkFaceIntersection(
			ray->origin, ray->dir, a, b, c, entryDistance, *exitDistance
		);

		if( r > -1.0f ) {
			*exitDistance = r;

			if( r <= 1.0f ) {
				return true;
			}
		}
	}

	return false;
}


/**
 * Traverse down the kd-tree to find a leaf node the given ray intersects.
 * @param  {int}                     nodeIndex
 * @param  {const global kdNonLeaf*} kdNonLeaves
 * @param  {const float4}            hitNear
 * @return {int}
 */
int goToLeafNode( int nodeIndex, const global kdNonLeaf* kdNonLeaves, const float4 hitNear ) {
	float4 split;
	int4 children;

	int axis = kdNonLeaves[nodeIndex].split.w;
	float hitPos[3] = { hitNear.x, hitNear.y, hitNear.z };
	float splitPos[3];
	bool isOnLeft;

	while( true ) {
		children = kdNonLeaves[nodeIndex].children;
		split = kdNonLeaves[nodeIndex].split;

		splitPos[0] = split.x;
		splitPos[1] = split.y;
		splitPos[2] = split.z;

		isOnLeft = ( hitPos[axis] < splitPos[axis] );
		nodeIndex = isOnLeft ? children.x : children.y;

		if( ( isOnLeft && children.z ) || ( !isOnLeft && children.w ) ) {
			return nodeIndex;
		}

		axis = MOD_3[axis + 1];
	}

	return -1;
}



/**
 * Find the closest hit of the ray with a surface.
 * Uses stackless kd-tree traversal.
 * @param {const float4*}       origin
 * @param {const float4*}       dir
 * @param {int}                 nodeIndex
 * @param {const global float*} kdNodeSplits
 * @param {const global float*} kdNodeBB
 * @param {const global int*}   kdNodeMeta
 * @param {const global float*} kdNodeFaces
 * @param {const global int*}   kdNodeRopes
 * @param {hit_t*}              result
 * @param {const int}           bounce
 * @param {const int}           kdRoot
 * @param {const float}         entryDistance
 * @param {const float}         exitDistance
 */
void traverseKdTree(
	ray4* ray, int nodeIndex, const global kdNonLeaf* kdNonLeaves,
	const global kdLeaf* kdLeaves, const global uint* kdFaces,
	const global float4* scVertices, const global uint4* scFaces,
	const int bounce, float entryDistance, float exitDistance
) {
	kdLeaf currentNode;
	int8 ropes;
	int exitRope = 0;
	float boxExitLimit = FLT_MAX;

	while( nodeIndex >= 0 && entryDistance < exitDistance ) {
		currentNode = kdLeaves[nodeIndex];
		ropes = currentNode.ropes;
		boxExitLimit = getBoxExitLimit( ray->origin, ray->dir, currentNode.bbMin, currentNode.bbMax );

		checkFaces(
			ray, nodeIndex, ropes.s6, ropes.s7, kdFaces,
			scVertices, scFaces, entryDistance, &exitDistance, boxExitLimit
		);

		// Exit leaf node
		updateEntryDistanceAndExitRope(
			ray->origin, ray->dir, currentNode.bbMin, currentNode.bbMax,
			&entryDistance, &exitRope
		);

		nodeIndex = ( (int*) &ropes )[exitRope];
		nodeIndex = ( nodeIndex < 1 )
		          ? -nodeIndex - 1
		          : goToLeafNode( nodeIndex - 1, kdNonLeaves, fma( entryDistance, ray->dir, ray->origin ) );
	}
}


/**
 * Test if from the current hit location there is an unobstracted direct path to the light source.
 * @param  {const float4*}       origin
 * @param  {const float4*}       dir
 * @param  {int}                 nodeIndex
 * @param  {const global float*} kdNodeSplits
 * @param  {const global float*} kdNodeBB
 * @param  {const global int*}   kdNodeMeta
 * @param  {const global float*} kdNodeFaces
 * @param  {const global int*}   kdNodeRopes
 * @param  {const float}         exitDistance
 * @return {bool}
 */
bool shadowTestIntersection(
	ray4* ray, const global kdNonLeaf* kdNonLeaves,
	const global kdLeaf* kdLeaves, const global float* kdFaces,
	const global float4* scVertices, const global uint4* scFaces
) {
	kdLeaf currentNode;
	int8 ropes;
	int exitRope;
	int nodeIndex = ray->nodeIndex;
	float entryDistance = 0.0f;
	float exitDistance = 1.0f;

	while( nodeIndex >= 0 && entryDistance < exitDistance ) {
		currentNode = kdLeaves[nodeIndex];
		ropes = currentNode.ropes;

		if( checkFacesForShadow(
			ray, nodeIndex, ropes.s6, ropes.s7, kdFaces,
			scVertices, scFaces, entryDistance, &exitDistance
		) ) {
			return true;
		}

		// Exit leaf node
		updateEntryDistanceAndExitRope(
			ray->origin, ray->dir, currentNode.bbMin, currentNode.bbMax,
			&entryDistance, &exitRope
		);

		nodeIndex = ( (int*) &ropes )[exitRope];
		nodeIndex = ( nodeIndex < 1 )
		          ? -nodeIndex - 1
		          : goToLeafNode( nodeIndex - 1, kdNonLeaves, fma( entryDistance, ray->dir, ray->origin ) );
	}

	return false;
}
