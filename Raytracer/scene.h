#pragma once
#define MT_LAMB 0
#define MT_METAL 1

#define MAX_SPHERES 16
#define MAX_PLANES 16
#define MAX_AABBS 16
#define MAX_MATERIALS 8

struct Material {
	glm::vec3 albedo;
	float roughness = 0;
	int32_t type = -1;
	int32_t padding[3];
};

struct Sphere {
	glm::vec4 position = glm::vec4(0);
	float radius = 0;
	int32_t material_index;
	int32_t padding[2] = {};
};

struct Plane {
	glm::vec4 A, B, C;
	int32_t material_index;
	int32_t padding[3];
};

struct AABB {
	glm::vec4 min_point, max_point;
	int32_t material_index;
	int32_t padding[3];
};

struct SceneData {
	Sphere spheres[MAX_SPHERES];
	Plane planes[MAX_PLANES];
	AABB AABBs[MAX_AABBS];
	Material materials[8];
	int32_t sphere_num = 0;
	int32_t plane_num = 0;
	int32_t box_num = 0;
	int32_t padding;
};

struct Camera {
	glm::vec4 position;
	glm::mat4 inverse_view;
	glm::mat4 inverse_projection;
	uint32_t frame_index = 0;
	int32_t padding[3];
};

// !!! Frame index is updated in UpdateCamera() !!!
class Scene {
private:
	Window *m_wnd;
	ComPtr<ID3D11Buffer> m_scene_buffer;
	ComPtr<ID3D11Buffer> m_camera_buffer;

	Camera m_camera;
	glm::vec3 m_camera_position = glm::vec3(0, 1, 0);
	float m_camera_yaw = 0, m_camera_pitch = 0;
	float m_camera_speed = 0.01f;
	float m_camera_sensitivity = 0.01f;
	glm::vec3 m_up = glm::vec3(0, 1, 0);

	SceneData m_scene_data;

public:
	Scene(Window *wnd, const SceneData* scene_data = 0) {
		m_wnd = wnd;

		if (scene_data)
			memcpy(&m_scene_data, scene_data, sizeof(SceneData));

		HRESULT hr;
		D3D11_BUFFER_DESC buf_desc = {};
		D3D11_SUBRESOURCE_DATA data = {};

		// Create sphere buffer
		buf_desc.Usage = D3D11_USAGE_DYNAMIC;
		buf_desc.ByteWidth = sizeof(SceneData);
		buf_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		buf_desc.StructureByteStride = 0;
		data.pSysMem = &m_scene_data;
		hr = m_wnd->GetDevice()->CreateBuffer(&buf_desc, &data, &m_scene_buffer);
		
		// Create camera buffer
		buf_desc.Usage = D3D11_USAGE_DYNAMIC;
		buf_desc.ByteWidth = sizeof(Camera);
		buf_desc.StructureByteStride = 0;
		data.pSysMem = &m_camera;
		hr = m_wnd->GetDevice()->CreateBuffer(&buf_desc, &data, &m_camera_buffer);
	
		// Bind buffers
		m_wnd->GetContext()->CSSetConstantBuffers(0, 1, m_scene_buffer.GetAddressOf());
		m_wnd->GetContext()->CSSetConstantBuffers(1, 1, m_camera_buffer.GetAddressOf());
	}
	void UpdateCameraBuffer() {
		D3D11_MAPPED_SUBRESOURCE resource;
		m_wnd->GetContext()->Map(m_camera_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
		memcpy(resource.pData, &m_camera, sizeof(Camera));
		m_wnd->GetContext()->Unmap(m_camera_buffer.Get(), 0);
	}
	void UpdateSceneBuffer() {
		D3D11_MAPPED_SUBRESOURCE resource;
		HRESULT hr = m_wnd->GetContext()->Map(m_scene_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
		memcpy(resource.pData, &m_scene_data, sizeof(SceneData));
		m_wnd->GetContext()->Unmap(m_scene_buffer.Get(), 0);
	}
	void UpdateCamera(float delta_time) {
		// Keyboard
		glm::vec3 front;
		front.x = cos(m_camera_yaw) * cos(m_camera_pitch);
		front.y = sin(m_camera_pitch);
		front.z = sin(m_camera_yaw) * cos(m_camera_pitch);
		front = glm::normalize(front);
		glm::vec3 right = glm::normalize(glm::cross(m_up, front));
		if (m_wnd->GetInput()->GetKeyDown('W')) {
			m_camera_position += front * m_camera_speed * delta_time;
		}
		else if (m_wnd->GetInput()->GetKeyDown('S')) {
			m_camera_position -= front * m_camera_speed * delta_time;
		}
		if (m_wnd->GetInput()->GetKeyDown('A')) {
			m_camera_position += right * m_camera_speed * delta_time;
		}
		else if (m_wnd->GetInput()->GetKeyDown('D')) {
			m_camera_position -= right * m_camera_speed * delta_time;
		}
		m_camera.position = glm::vec4(m_camera_position, 1.0f);

		// Mouse
		m_camera_yaw += m_camera_sensitivity * m_wnd->GetInput()->GetDeltaMouseX();
		m_camera_pitch += -m_camera_sensitivity * m_wnd->GetInput()->GetDeltaMouseY();
		m_camera_pitch = glm::clamp(m_camera_pitch, -glm::half_pi<float>()+0.1f, glm::half_pi<float>()-0.1f);
	
		m_camera.inverse_view = glm::inverse(glm::lookAt(
			m_camera_position, m_camera_position + front, m_up
		));
		m_camera.inverse_projection = glm::inverse(glm::perspective(
			glm::radians(90.0f), (float)WND_WIDTH / WND_HEIGHT, 0.1f, 100.0f
		));

		if (m_camera.frame_index > 1000000) m_camera.frame_index = 0;
		m_camera.frame_index++;
	}
public:
	/*
	* Scene file format:
	* material [albedo] [type] [roughness]
	* sphere [position] [radius] [material index]
	*/
	void Save(const char* filename) {
		std::ofstream file(filename, std::ofstream::trunc);
		// Materials
		for (int i = 0; i < MAX_MATERIALS; i++) {
			if (m_scene_data.materials[i].type == -1) break;
			file << "material " <<
				m_scene_data.materials[i].albedo.x << " " <<
				m_scene_data.materials[i].albedo.y << " " <<
				m_scene_data.materials[i].albedo.z << " " <<
				m_scene_data.materials[i].type << " " <<
				m_scene_data.materials[i].roughness << " " << "\n";
		}
		// Spheres
		for (int i = 0; i < m_scene_data.sphere_num; i++) {
			file << "sphere " <<
				m_scene_data.spheres[i].position.x << " " <<
				m_scene_data.spheres[i].position.y << " " <<
				m_scene_data.spheres[i].position.z << " " <<
				m_scene_data.spheres[i].radius << " " <<
				m_scene_data.spheres[i].material_index << "\n";
		}
	}
	// Returns false on failure
	bool Load(const char* filename) {
		std::ifstream file(filename);
		if (!file.is_open()) return 0;

		uint8_t material_num = 0;
		std::string str;
		while (file >> str) {
			if (str == "material") {
				Material mat;
				file >> mat.albedo.x >> mat.albedo.y >> mat.albedo.z >>
					mat.type >> mat.roughness;
				m_scene_data.materials[material_num] = mat;
				material_num++;
			}
			else if (str == "sphere") {
				Sphere sphere;
				file >> sphere.position.x >> sphere.position.y >> sphere.position.z >>
					sphere.radius >> sphere.material_index;
				m_scene_data.spheres[m_scene_data.sphere_num++] = sphere;
			}
		}

		UpdateSceneBuffer();

		return 1;
	}
};
