#include "Camera.h"
#include "../GUI/GUI.h"
#include "../VulkanContext/VulkanContext.h"

using namespace vm;

const std::array<cvec2, 16> halton16{
	cvec2(0.5f, 0.3333333333333333f),
	cvec2(0.25f, 0.6666666666666666f),
	cvec2(0.75f, 0.1111111111111111f),
	cvec2(0.125f, 0.4444444444444444f),
	cvec2(0.625f, 0.7777777777777778f),
	cvec2(0.375f, 0.2222222222222222f),
	cvec2(0.875f, 0.5555555555555556f),
	cvec2(0.0625f, 0.8888888888888888f),
	cvec2(0.5625f, 0.037037037037037035f),
	cvec2(0.3125f, 0.37037037037037035f),
	cvec2(0.8125f, 0.7037037037037037f),
	cvec2(0.1875f, 0.14814814814814814f),
	cvec2(0.6875f, 0.48148148148148145f),
	cvec2(0.4375f, 0.8148148148148148f),
	cvec2(0.9375f, 0.25925925925925924f),
	cvec2(0.03125f, 0.5925925925925926f)
};
const std::array<cvec2, 32> halton32{
	cvec2(0.53125f, 0.9259259259259259f),
	cvec2(0.28125f, 0.07407407407407407f),
	cvec2(0.78125f, 0.4074074074074074f),
	cvec2(0.15625f, 0.7407407407407407f),
	cvec2(0.65625f, 0.18518518518518517f),
	cvec2(0.40625f, 0.5185185185185185f),
	cvec2(0.90625f, 0.8518518518518519f),
	cvec2(0.09375f, 0.2962962962962963f),
	cvec2(0.59375f, 0.6296296296296297f),
	cvec2(0.34375f, 0.9629629629629629f),
	cvec2(0.84375f, 0.012345679012345678f),
	cvec2(0.21875f, 0.345679012345679f),
	cvec2(0.71875f, 0.6790123456790124f),
	cvec2(0.46875f, 0.12345679012345678f),
	cvec2(0.96875f, 0.4567901234567901f),
	cvec2(0.015625f, 0.7901234567901234f),
	cvec2(0.515625f, 0.2345679012345679f),
	cvec2(0.265625f, 0.5679012345679012f),
	cvec2(0.765625f, 0.9012345679012346f),
	cvec2(0.140625f, 0.04938271604938271f),
	cvec2(0.640625f, 0.38271604938271603f),
	cvec2(0.390625f, 0.7160493827160493f),
	cvec2(0.890625f, 0.16049382716049382f),
	cvec2(0.078125f, 0.49382716049382713f),
	cvec2(0.578125f, 0.8271604938271605f),
	cvec2(0.328125f, 0.2716049382716049f),
	cvec2(0.828125f, 0.6049382716049383f),
	cvec2(0.203125f, 0.9382716049382716f),
	cvec2(0.703125f, 0.08641975308641975f),
	cvec2(0.453125f, 0.41975308641975306f),
	cvec2(0.953125f, 0.7530864197530864f),
	cvec2(0.046875f, 0.19753086419753085f)
};

const auto& halton = halton16;

Camera::Camera()
{
	// gltf is right handed, reversing the x orientation makes the models left handed
	// reversing the y axis to match the vulkan y axis too
	worldOrientation = vec3(-1.f, -1.f, 1.f);

	// total pitch, yaw, roll
	euler = vec3(0.f, radians(180.f), 0.f);
	orientation = quat(euler);
	position = vec3(0.f, 0.f, 0.f);

	nearPlane = 500.0f;
	farPlane = 0.005f;
	FOV = 45.0f;
	speed = 0.35f;
	rotationSpeed = 0.05f;

	renderArea.viewport.x = GUI::winPos.x;
	renderArea.viewport.y = GUI::winPos.y;
	renderArea.viewport.width = GUI::winSize.x;
	renderArea.viewport.height = GUI::winSize.y;
	renderArea.viewport.minDepth = 0.f;
	renderArea.viewport.maxDepth = 1.f;
	renderArea.scissor.offset.x = static_cast<int32_t>(GUI::winPos.x);
	renderArea.scissor.offset.y = static_cast<int32_t>(GUI::winPos.y);
	renderArea.scissor.extent.width = static_cast<int32_t>(GUI::winSize.x);
	renderArea.scissor.extent.height = static_cast<int32_t>(GUI::winSize.y);
}

void vm::Camera::update()
{
	renderArea.update(vec2(&GUI::winPos.x), vec2(&GUI::winSize.x));
	front = orientation * worldFront();
	right = orientation * worldRight();
	up = orientation * worldUp();
	previousView = view;
	previousProjection = projection; 
	projOffsetPrevious = projOffset;
	if (GUI::use_TAA) {
		static uint32_t haltonCounter = 0;
		projOffset.x = ((halton[haltonCounter].x * 2.0f - 1.0f) / WIDTH_f) * GUI::TAA_jitter_scale;
		projOffset.y = ((halton[haltonCounter].y * 2.0f - 1.0f) / HEIGHT_f) * GUI::TAA_jitter_scale;
		haltonCounter = (++haltonCounter) % halton.size();
	}
	else {
		projOffset = { 0.0f, 0.0f };
	}
	updatePerspective();
	updateView();
	invView = inverse(view);
	invProjection = inverse(projection);
	invViewProjection = invView * invProjection;
	ExtractFrustum();
}

void vm::Camera::updatePerspective()
{
	const float aspect = renderArea.viewport.width / renderArea.viewport.height;
	const float tanHalfFovy = tan(radians(FOV) * .5f);
	const float m00 = 1.f / (aspect * tanHalfFovy);
	const float m11 = 1.f / (tanHalfFovy);
	const float m22 = farPlane / (farPlane - nearPlane) * worldOrientation.z;
	const float m23 = worldOrientation.z;
	const float m32 = -(farPlane * nearPlane) / (farPlane - nearPlane);
	const float m20 = projOffset.x;
	const float m21 = projOffset.y;
	projection = mat4(
		m00, 0.f, 0.f, 0.f,
		0.f, m11, 0.f, 0.f,
		m20, m21, m22, m23,
		0.f, 0.f, m32, 0.f
	);
}

void vm::Camera::updateView()
{
	const vec3& r = right;
	const vec3& u = up;
	const vec3& f = front;

	const float m30 = -dot(r, position);
	const float m31 = -dot(u, position);
	const float m32 = -dot(f, position);

	view = mat4(
		r.x, u.x, f.x, 0.f,
		r.y, u.y, f.y, 0.f,
		r.z, u.z, f.z, 0.f,
		m30, m31, m32, 1.f
	);
}

void Camera::move(RelativeDirection direction, float velocity)
{
	if (direction == RelativeDirection::FORWARD)	position += front * (velocity * worldOrientation.z);
	if (direction == RelativeDirection::BACKWARD)	position -= front * (velocity * worldOrientation.z);
	if (direction == RelativeDirection::RIGHT)		position += right * velocity;
	if (direction == RelativeDirection::LEFT)		position -= right * velocity;
}

void Camera::rotate(float xoffset, float yoffset)
{
	yoffset *= rotationSpeed;
	xoffset *= rotationSpeed;
	euler.x += radians(-yoffset) * worldOrientation.y;	// pitch
	euler.y += radians(xoffset) * worldOrientation.x;	// yaw

	orientation = quat(euler);
}

vec3 Camera::worldRight() const
{
	return vec3(worldOrientation.x, 0.f, 0.f);
}

vec3 Camera::worldUp() const
{
	return vec3(0.f, worldOrientation.y, 0.f);
}

vec3 Camera::worldFront() const
{
	return vec3(0.f, 0.f, worldOrientation.z);
}

void Camera::ExtractFrustum()
{
	// transpose just to make the calculations look simpler
	mat4 pvm = transpose(projection * view);
	vec4 temp;

	/* Extract the numbers for the RIGHT plane */
	temp = pvm[3] - pvm[0];
	temp /= length(vec3(temp));

	frustum[0].normal = vec3(temp);
	frustum[0].d = temp.w;

	/* Extract the numbers for the LEFT plane */
	temp = pvm[3] + pvm[0];
	temp /= length(vec3(temp));

	frustum[1].normal = vec3(temp);
	frustum[1].d = temp.w;

	/* Extract the BOTTOM plane */
	temp = pvm[3] + pvm[1];
	temp /= length(vec3(temp));

	frustum[2].normal = vec3(temp);
	frustum[2].d = temp.w;

	/* Extract the TOP plane */
	temp = pvm[3] - pvm[1];
	temp /= length(vec3(temp));

	frustum[3].normal = vec3(temp);
	frustum[3].d = temp.w;

	/* Extract the FAR plane */
	temp = pvm[3] - pvm[2];
	temp /= length(vec3(temp));

	frustum[4].normal = vec3(temp);
	frustum[4].d = temp.w;

	/* Extract the NEAR plane */
	temp = pvm[3] + pvm[2];
	temp /= length(vec3(temp));

	frustum[5].normal = vec3(temp);
	frustum[5].d = temp.w;
}

// center x,y,z - radius w 
bool Camera::SphereInFrustum(const vec4& boundingSphere) const
{
	for (unsigned i = 0; i < 6; i++) {
		const float dist = dot(frustum[i].normal, vec3(boundingSphere)) + frustum[i].d;

		if (dist < -boundingSphere.w)
			return false;

		if (fabs(dist) < boundingSphere.w)
			return true;
	}
	return true;
}

void Camera::SurfaceTargetArea::update(const vec2& position, const vec2& size, float minDepth, float maxDepth)
{
	viewport.x = position.x;
	viewport.y = position.y;
	viewport.width = size.x;
	viewport.height = size.y;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;

	scissor.offset.x = static_cast<int32_t>(position.x);
	scissor.offset.y = static_cast<int32_t>(position.y);
	scissor.extent.width = static_cast<int32_t>(size.x);
	scissor.extent.height = static_cast<int32_t>(size.y);
}
