#define MT_LAMB 0
#define MT_METAL 1

#define MAX_SPHERES 16
#define MAX_PLANES 16
#define MAX_AABBS 16
#define MAX_MATERIALS 8

RWTexture2D<float4> output : register(u0);

struct Material {
	float3 albedo;
	float roughness;
	int type;
	int3 padding;
};

struct Sphere {
	float4 center;
	float radius;
	int material_index;
	int2 padding;
};

struct Plane {
	float4 A, B, C;
	int material_index;
	int3 padding;
};

struct AABB {
	float4 min_point, max_point;
	int material_index;
	int3 padding;
};

cbuffer Scene : register(b0) {
	Sphere spheres[MAX_SPHERES];
	Plane planes[MAX_PLANES];
	AABB AABBs[MAX_AABBS];
	Material materials[MAX_MATERIALS];
	int sphere_num = 0;
	int plane_num = 0;
	int box_num = 0;
	int padding;
};

cbuffer Camera : register(b1) {
	float4 position;
	float4x4 inverse_view;
	float4x4 inverse_projection;
	uint frame_index;
};

struct Ray {
	float3 origin;
	float3 direction;
};
struct Hit {
	float3 position;
	float3 normal;
	int material_index;
	bool did_hit;
};

float Random(inout float2 state) {
	float2 noise = (frac(sin(dot(state, float2(12.9898, 78.233) * 2.0)) * 43758.5453));
	state.x *= 1.0000233490586;
	state.y *= 1.0000249578329;
	return abs(noise.x + noise.y) - 1.0;
}
float RandomNormalDist(inout float2 state) {
	float theta = 2 * 3.1415926;
	float rho = sqrt(-2 * log(Random(state)));
	return rho * cos(theta);
}

float3 RandomVector(inout float2 state) {
	float3 result;
	result.x = Random(state);
	result.y = Random(state);
	result.z = Random(state);
	return normalize(result);
}

float3 RandomVectorHemisphere(inout float2 state, float3 normal) {
	float3 dir = RandomVector(state);
	return dir * sign(dot(normal, dir));
}

Hit HitSphere(Ray r, Sphere s) {
	Hit hit;
	hit.did_hit = 0;

	float3 oc = r.origin - s.center.xyz;
	float a = length(r.direction) * length(r.direction);
	float half_b = dot(oc, r.direction);
	float c = length(oc)*length(oc) - s.radius * s.radius;

	float discriminant = half_b * half_b - a * c;
	if (discriminant < 0) return hit;
	float sqrtd = sqrt(discriminant);

	// Find the nearest root that lies in the acceptable range.
	float root = (-half_b - sqrtd) / a;
	if (root <= 0) return hit;

	hit.did_hit = true;
	hit.position = r.origin + root * r.direction;
	hit.normal = (hit.position - s.center.xyz) / s.radius;
	hit.material_index = s.material_index;
	
	return hit;
}

Hit RayCollision(Ray ray) {
	float min_dst = 1000.0f;
	Hit hit;
	hit.did_hit = 0;
	for (int i = 0; i < sphere_num; i++) {
		Hit h = HitSphere(ray, spheres[i]);
		if (h.did_hit && length(h.position - position.xyz) < min_dst) {
			hit = h;
			min_dst = length(hit.position - position.xyz);
		}
	}
	return hit;
}

float3 TraceRay(Ray ray, inout float2 state) {
	const int max_bounces = 5;
	
	const float3 sky_color = float3(0.4, 0.6, 0.8);
	float3 color = 1;
	float multiplier = 1.0f;
	for (int i = 0; i < max_bounces; i++) {

		Hit hit = RayCollision(ray);
		if (!hit.did_hit) {
			float3 unit_direction = normalize(ray.direction);
			float t = 0.5 * (unit_direction.y + 1.0);
			color *= (1.0 - t) * float3(1.0, 1.0, 1.0) + t * float3(0.5, 0.7, 1.0);
			break;
		}
		
		// Works for lambertien and also metal materials
		color *= materials[hit.material_index].albedo*multiplier;

		ray.origin = hit.position + hit.normal * 0.0001f;
		
		if (materials[hit.material_index].type == MT_LAMB) {
			ray.direction = normalize(RandomVector(state) + hit.normal);
		}
		else if (materials[hit.material_index].type == MT_METAL) {
			ray.direction = normalize(reflect(ray.direction, hit.normal) + +materials[hit.material_index].roughness * RandomVectorHemisphere(state, hit.normal));
		}

		multiplier *= 0.5f;
	}
	return (color);
}

static const int thread_num = 16;
[numthreads(thread_num, thread_num, 1)]
void main(int3 id : SV_DispatchThreadID) {
	// Image
	const int wnd_width = 1280, wnd_height = 960;
	const float aspect_ratio = float(wnd_width) / float(wnd_height);

	// Camera
	float viewport_height = 2.0;
	float viewport_width = aspect_ratio * viewport_height;
	float focal_length = 1.0;

	float3 horizontal = float3(viewport_width, 0, 0);
	float3 vertical = float3(0, viewport_height, 0);
	float3 lower_left_corner = horizontal / 2 - vertical / 2 - float3(0, 0, focal_length);

	float u = (float(id.x) / wnd_width) * 2 - 1;
	float v = (float(id.y) / wnd_height) * 2 - 1;

	Ray ray;
	ray.origin = position.xyz;
	float4 target = mul(inverse_projection, float4(u, v, 1, 1));
	ray.direction = mul(inverse_view, float4(normalize(target.xyz / target.w), 0)).xyz;

	const int sample_num = 32;
	float3 output_color = 0;
	float2 state = id.xy;
	for (int i = 0; i < sample_num; i++) {
		Ray r = ray;
		r.direction += (2.0f / wnd_height) * RandomVector(state);
		output_color += TraceRay(r, state);
	}
	float scale = 1.0 / sample_num;
	output_color.x = sqrt(scale * output_color.x);
	output_color.y = sqrt(scale * output_color.y);
	output_color.z = sqrt(scale * output_color.z);

	output[id.xy] = float4(output_color, 1);
}
