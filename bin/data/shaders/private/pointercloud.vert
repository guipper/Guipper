#version 330

// PointerCloud - metric point cloud.
//
// Each vertex carries its own depth-image pixel coordinate in position.xy.
// The mesh is static: density decimation and near/far clipping happen here by
// pushing rejected points outside the clip volume, so the CPU never rebuilds
// or re-uploads geometry.

uniform sampler2D depthTexture;   // R32F, raw libfreenect2 millimetres
uniform sampler2D colorTexture;   // optional colour source
uniform vec2 depthSize;           // 512 x 424
uniform vec2 targetSize;          // fbo size, for aspect correction

// IR camera pinhole parameters, straight from the device.
uniform float fx;
uniform float fy;
uniform float cx;
uniform float cy;

uniform float nearMm;
uniform float farMm;
uniform int densityStep;          // keep 1 pixel out of every densityStep
uniform int upsample;             // points per depth pixel along each axis
uniform float pointSize;
uniform float depthScale;
uniform float rotateX;
uniform float rotateY;
uniform float rotateZ;
uniform float zoom;
uniform float fovScale;           // 0 = orthographic, 1 = strong perspective
uniform int mirror;
uniform int flipVertical;
uniform int hasColor;

in vec4 position;                 // (column, row, sub-point index)

out float vDepthNorm;             // 0 at nearMm, 1 at farMm
out vec3 vColor;
out float vHasColor;

const vec3 kRejected = vec3(2.0, 2.0, 2.0);

mat3 rotationX(float a)
{
	float c = cos(a);
	float s = sin(a);
	return mat3(1.0, 0.0, 0.0,
	            0.0,   c,  -s,
	            0.0,   s,   c);
}

mat3 rotationY(float a)
{
	float c = cos(a);
	float s = sin(a);
	return mat3(  c, 0.0,   s,
	            0.0, 1.0, 0.0,
	             -s, 0.0,   c);
}

mat3 rotationZ(float a)
{
	float c = cos(a);
	float s = sin(a);
	return mat3(  c,  -s, 0.0,
	              s,   c, 0.0,
	            0.0, 0.0, 1.0);
}

void reject()
{
	gl_Position = vec4(kRejected, 1.0);
	gl_PointSize = 0.0;
	vDepthNorm = 0.0;
	vColor = vec3(0.0);
	vHasColor = 0.0;
}

// Bilinear between the four surrounding depth pixels, but only across a
// continuous surface: where they disagree by more than a few percent this is
// a silhouette edge, and a blend would invent a point floating between the
// person and the wall - so take the nearest pixel instead. The texture stays
// GL_NEAREST; filtering here is explicit.
float sampleDepth(vec2 pixel)
{
	ivec2 size = ivec2(depthSize);
	ivec2 base = ivec2(floor(pixel));
	vec2 t = pixel - vec2(base);
	ivec2 p00 = clamp(base, ivec2(0), size - 1);
	ivec2 p11 = clamp(base + 1, ivec2(0), size - 1);
	float d00 = texelFetch(depthTexture, p00, 0).r;
	float d10 = texelFetch(depthTexture, ivec2(p11.x, p00.y), 0).r;
	float d01 = texelFetch(depthTexture, ivec2(p00.x, p11.y), 0).r;
	float d11 = texelFetch(depthTexture, p11, 0).r;
	float lo = min(min(d00, d10), min(d01, d11));
	float hi = max(max(d00, d10), max(d01, d11));
	if (lo <= 0.0 || hi - lo > 0.04 * lo + 20.0)
	{
		ivec2 nearest = clamp(ivec2(floor(pixel + 0.5)), ivec2(0), size - 1);
		return texelFetch(depthTexture, nearest, 0).r;
	}
	return mix(mix(d00, d10, t.x), mix(d01, d11, t.x), t.y);
}

void main()
{
	int column = int(position.x);
	int row = int(position.y);
	int sub = int(position.z);
	int k = max(1, upsample);
	// Sub-points spread evenly over the pixel; with k = 1 this is the pixel
	// itself, exactly as before upsampling existed.
	vec2 pixel = vec2(column, row) +
		(vec2(sub % k, sub / k) + 0.5) / float(k) - 0.5;

	// Decimation. Dropping whole rows and columns keeps the surviving points
	// on a regular lattice, which reads far better than a random subset.
	if (densityStep > 1 &&
		(column % densityStep != 0 || row % densityStep != 0))
	{
		reject();
		return;
	}

	vec2 uv = (pixel + 0.5) / depthSize;
	float depthMm = sampleDepth(pixel);

	// libfreenect2 writes 0 for pixels it could not resolve.
	if (depthMm <= 0.0 || depthMm < nearMm || depthMm > farMm)
	{
		reject();
		return;
	}

	vDepthNorm = clamp((depthMm - nearMm) / max(1.0, farMm - nearMm), 0.0, 1.0);

	// Unproject through the IR pinhole into camera-space millimetres. This is
	// libfreenect2's getPointXYZ convention.
	vec3 point = vec3(
		(pixel.x + 0.5 - cx) / fx * depthMm,
		(pixel.y + 0.5 - cy) / fy * depthMm,
		depthMm);

	// Metres, and centred on the middle of the working volume so rotation
	// pivots through the subject rather than the camera.
	point *= 0.001;
	point.y = -point.y;
	point.z -= (nearMm + farMm) * 0.0005;
	point.z *= depthScale;

	if (mirror != 0) point.x = -point.x;
	if (flipVertical != 0) point.y = -point.y;

	point = rotationY(rotateY) * rotationX(rotateX) * rotationZ(rotateZ) * point;

	// Perspective divide, blended towards orthographic as fovScale falls to 0.
	float w = mix(1.0, 1.0 + point.z * 0.5, fovScale);
	float projected = 1.0 / max(0.05, w);

	float aspect = targetSize.x / max(1.0, targetSize.y);
	gl_Position = vec4(
		point.x * zoom * projected / aspect,
		point.y * zoom * projected,
		clamp(point.z * 0.05, -0.999, 0.999),
		1.0);

	// Nearer points are bigger, and points shrink as the cloud recedes.
	// Not divided by the upsample factor: at the phone's density that makes
	// sub-pixel points, which alias into a moire of rings. Extra points close
	// the gaps instead; point size stays the user's.
	gl_PointSize = max(1.0, pointSize * projected);

	vHasColor = hasColor != 0 ? 1.0 : 0.0;
	vColor = hasColor != 0 ?
		textureLod(colorTexture, vec2(uv.x, 1.0 - uv.y), 0.0).rgb :
		vec3(0.0);
}
