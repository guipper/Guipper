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

in vec4 position;                 // (column, row, 0)

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

void main()
{
	vec2 pixel = position.xy;
	int column = int(pixel.x);
	int row = int(pixel.y);

	// Decimation. Dropping whole rows and columns keeps the surviving points
	// on a regular lattice, which reads far better than a random subset.
	if (densityStep > 1 &&
		(column % densityStep != 0 || row % densityStep != 0))
	{
		reject();
		return;
	}

	// textureLod, not texture: LOD is not implicitly computed outside the
	// fragment stage.
	vec2 uv = (pixel + 0.5) / depthSize;
	float depthMm = textureLod(depthTexture, uv, 0.0).r;

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
	gl_PointSize = max(1.0, pointSize * projected);

	vHasColor = hasColor != 0 ? 1.0 : 0.0;
	vColor = hasColor != 0 ?
		textureLod(colorTexture, vec2(uv.x, 1.0 - uv.y), 0.0).rgb :
		vec3(0.0);
}
