#include "scene_mgr.h"
#include "vk_utils.h"
#include "../loader_utils/gltf_utils.h"

#define TINYGLTF_IMPLEMENTATION
//#define TINYGLTF_NO_STB_IMAGE_WRITE
//#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE
//#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_USE_CPP14
#include "tiny_gltf.h"


bool SceneManager::InitEmptyScene(uint32_t maxMeshes, uint32_t maxTotalVertices, uint32_t maxTotalPrimitives, uint32_t maxPrimitivesPerMesh)
{
  m_pMeshData = std::make_shared<Mesh8F>();
  InitGeoBuffersGPU(maxMeshes, maxTotalVertices, maxTotalPrimitives * 3);
  if(m_config.build_acc_structs)
  {
    m_pBuilderV2->Init(maxTotalVertices, maxPrimitivesPerMesh, maxTotalPrimitives, m_pMeshData->SingleVertexSize());
  }

  return true;
}

bool SceneManager::LoadScene(const std::string &scenePath)
{
  auto found = scenePath.find_last_of('.');
  if(found == std::string::npos)
  {
    RUN_TIME_ERROR("Can't guess scene format");
  }

  std::string ext = scenePath.substr(scenePath.find_last_of('.'), scenePath.size());

  if(ext == ".gltf")
    return LoadSceneGLTF(scenePath);
  else if(ext == ".xml")
    return LoadSceneXML(scenePath, false);

  return false;
}

bool SceneManager::LoadSceneXML(const std::string &scenePath, bool transpose)
{
  tinygltf::Model gltfVehModel;
  std::string vehicleFolder;
  auto hscene_main = std::make_shared<hydra_xml::HydraScene>();
  auto res         = hscene_main->LoadState(scenePath);

  if(res < 0)
  {
    RUN_TIME_ERROR("LoadSceneXML error");
    return false;
  }

  m_pMeshData = std::make_shared<Mesh8F>();

  uint32_t maxVertexCountPerMesh    = 0u;
  uint32_t maxPrimitiveCountPerMesh = 0u;
  uint32_t totalPrimitiveCount      = 0u;
  uint32_t totalVerticesCount       = 0u;
  uint32_t totalMeshes              = 0u;
  if(m_config.load_geometry)
  {
    for(auto mesh_node : hscene_main->GeomNodes())
    {
      uint32_t vertNum = mesh_node.attribute(L"vertNum").as_int();
      uint32_t primNum = mesh_node.attribute(L"triNum").as_int();
      maxVertexCountPerMesh    = std::max(vertNum, maxVertexCountPerMesh);
      maxPrimitiveCountPerMesh = std::max(primNum, maxPrimitiveCountPerMesh);
      totalVerticesCount      += vertNum;
      totalPrimitiveCount     += primNum;
      totalMeshes++;
    }

    loadVehicleFromFile(modelPath, gltfVehModel, vehicleFolder);

    maxVertexCountPerMesh    = std::max((uint32_t)8, maxVertexCountPerMesh);
    maxPrimitiveCountPerMesh = std::max((uint32_t)12, maxPrimitiveCountPerMesh); // this just to be working 

    for(const auto& mesh : gltfVehModel.meshes)
    {
      uint32_t vertNum = 0;
      uint32_t indexNum = 0;
      getNumVerticesAndIndicesFromGLTFMesh(gltfVehModel, mesh, vertNum, indexNum);
      maxVertexCountPerMesh    = std::max(vertNum, maxVertexCountPerMesh);
      maxPrimitiveCountPerMesh = std::max(indexNum / 3, maxPrimitiveCountPerMesh);
      totalVerticesCount      += vertNum;
      totalPrimitiveCount     += indexNum / 3;
      totalMeshes++;
    }

    maxVertexCountPerMesh    = std::max((uint32_t)8, maxVertexCountPerMesh);
    maxPrimitiveCountPerMesh = std::max((uint32_t)12, maxPrimitiveCountPerMesh); // this just to be working 

    InitGeoBuffersGPU(totalMeshes, totalVerticesCount, totalPrimitiveCount * 3);
    if(m_config.build_acc_structs)
    {
      m_pBuilderV2->Init(maxVertexCountPerMesh, maxPrimitiveCountPerMesh, totalPrimitiveCount, m_pMeshData->SingleVertexSize(),
        m_config.build_acc_structs_while_loading_scene);
    }
    for(auto loc : hscene_main->MeshFiles())
    {
      auto meshId = AddMeshFromFile(loc);

      if(m_config.debug_output)
        std::cout << "Loading mesh # " << meshId << std::endl;

      LoadOneMeshOnGPU(meshId);
      if(m_config.build_acc_structs)
      {
        AddBLAS(meshId);
      }

      auto instances = hscene_main->GetAllInstancesOfMeshLoc(loc);
      for(size_t j = 0; j < instances.size(); ++j)
      {
        if(transpose)
          InstanceMesh(meshId, LiteMath::transpose(instances[j]));
        else
          InstanceMesh(meshId, instances[j]);
      }
    }

    //load vehicle here
    //meshCounter = loaded_meshes_to_meshId.size();
    std::unordered_map<int, uint32_t> vehloaded_meshes_to_meshId;
    const tinygltf::Scene& scene = gltfVehModel.scenes[0];
    for(size_t i = 0; i < scene.nodes.size(); ++i)
    {
      const tinygltf::Node node = gltfVehModel.nodes[scene.nodes[0]];
      auto identity = m_currVehicleInstanceMatrices[0];
      LoadGLTFNodesRecursive(gltfVehModel, node, identity, vehloaded_meshes_to_meshId, true);
    }
  }

  for(auto cam : hscene_main->Cameras())
  {
    m_sceneCameras.push_back(cam);
  }

  if(m_config.load_materials != MATERIAL_LOAD_MODE::NONE)
  {
    m_materials.reserve(32);
    for(auto gltfMat : hscene_main->MaterialsGLTF())
    {

      MaterialData_pbrMR mat = {};
      mat.baseColor   = LiteMath::float4(gltfMat.metRoughnessData.baseColor);
      mat.alphaCutoff = gltfMat.alphaCutoff;
      mat.alphaMode   = gltfMat.alphaMode;
      mat.metallic    = gltfMat.metRoughnessData.metallic;
      mat.roughness   = gltfMat.metRoughnessData.roughness;
      mat.emissionColor  = LiteMath::float3(gltfMat.emissionColor);
      mat.emissionTexId  = gltfMat.emissionTexId;
      mat.normalTexId    = gltfMat.normalTexId;
      mat.occlusionTexId = gltfMat.occlusionTexId;
      mat.baseColorTexId = gltfMat.metRoughnessData.baseColorTexId;
      mat.metallicRoughnessTexId = gltfMat.metRoughnessData.metallicRoughnessTexId;
      m_materials.push_back(mat);
    }

    m_dynMaterials.reserve(gltfVehModel.materials.size());
    for(const tinygltf::Material &gltfMat : gltfVehModel.materials)
    {
      MaterialData_pbrMR mat = materialDataFromGLTF(gltfMat);
      m_dynMaterials.push_back(mat);
    }
  }

  if(m_config.load_materials == MATERIAL_LOAD_MODE::MATERIALS_AND_TEXTURES)
  {
    m_textureInfos.reserve(m_materials.size() * 4);
    for(auto tex : hscene_main->TextureFiles())
    {
      ImageFileInfo texInfo = getImageInfo(tex);
      if(!texInfo.is_ok)
      {
        std::stringstream ss;
        ss << "Texture at \"" << tex << "\" is absent or corrupted." ;
        vk_utils::logWarning(ss.str());
      }
      m_textureInfos.push_back(texInfo);
    }

    m_dynTextureInfos.reserve(gltfVehModel.materials.size() * 4);
    for (tinygltf::Image &image : gltfVehModel.images)
    {
      auto texturePath      = vehicleFolder + image.uri;
      ImageFileInfo texInfo = getImageInfo(texturePath);
      if(!texInfo.is_ok)
      {
        std::stringstream ss;
        ss << "Texture at \"" << texturePath << "\" is absent or corrupted." ;
        vk_utils::logWarning(ss.str());
      }
      m_dynTextureInfos.push_back(texInfo);
    }
  }

  if(m_config.load_geometry)
  {
    LoadCommonGeoDataOnGPU();
  }

  if(m_config.instance_matrix_as_vertex_attribute | m_config.instance_matrix_as_storage_buffer)
  {
    LoadInstanceDataOnGPU();
  }

  if(m_config.load_materials != MATERIAL_LOAD_MODE::NONE)
  {
    LoadMaterialDataOnGPU();
  }

  hscene_main = nullptr;

  return true;
}

bool SceneManager::LoadSceneGLTF(const std::string &scenePath)
{
  tinygltf::Model gltfModel;
  tinygltf::Model gltfVehModel;
  tinygltf::TinyGLTF gltfContext;
  std::string error, warning;

  std::string sceneFolder;
  std::string vehicleFolder;
  auto found = scenePath.find_last_of('/');
  if(found != std::string::npos && found != scenePath.size())
    sceneFolder = scenePath.substr(0, found + 1);
  else
    sceneFolder = "./";

  bool loaded = gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, scenePath);

  if(!loaded)
  {
    std::stringstream ss;
    ss << "Cannot load glTF scene from: " << scenePath;
    vk_utils::logWarning(ss.str());

    return false;
  }

  const tinygltf::Scene& scene = gltfModel.scenes[0];

//  for(const auto& gltfCam : gltfModel.cameras)
//  {
//    // @TODOw
//    if(gltfCam.type == "perspective")
//    {
//      hydra_xml::Camera cam = {};
//      cam.fov       = gltfCam.perspective.yfov / DEG_TO_RAD;
//      cam.nearPlane = gltfCam.perspective.znear;
//      cam.farPlane  = gltfCam.perspective.zfar;
//      cam.pos[0] = 0.0f; cam.pos[1] = 0.0f; cam.pos[2] = 0.0f;
//      cam.lookAt[0] = 0.0f; cam.lookAt[1] = 0.0f; cam.lookAt[2] = -1.0f;
//      cam.up[0] = 0.0f; cam.up[1] = 1.0f; cam.up[2] = 0.0f;
//      m_sceneCameras.push_back(cam);
//    }
//    else if (gltfCam.type == "orthographic")
//    {
//
//    }
//
//  }

  m_pMeshData = std::make_shared<Mesh8F>();

  uint32_t maxVertexCountPerMesh    = 0u;
  uint32_t maxPrimitiveCountPerMesh = 0u;
  uint32_t totalPrimitiveCount      = 0u;
  uint32_t totalVerticesCount       = 0u;
  uint32_t totalMeshes              = 0u;
  if(m_config.load_geometry)
  {
    for(const auto& mesh : gltfModel.meshes)
    {
      uint32_t vertNum = 0;
      uint32_t indexNum = 0;
      getNumVerticesAndIndicesFromGLTFMesh(gltfModel, mesh, vertNum, indexNum);
      maxVertexCountPerMesh    = std::max(vertNum, maxVertexCountPerMesh);
      maxPrimitiveCountPerMesh = std::max(indexNum / 3, maxPrimitiveCountPerMesh);
      totalVerticesCount      += vertNum;
      totalPrimitiveCount     += indexNum / 3;
      totalMeshes++;
    }
    maxVertexCountPerMesh    = std::max((uint32_t)8, maxVertexCountPerMesh);
    maxPrimitiveCountPerMesh = std::max((uint32_t)12, maxPrimitiveCountPerMesh); // this just to be working 

    loadVehicleFromFile(modelPath, gltfVehModel, vehicleFolder);

    for(const auto& mesh : gltfVehModel.meshes)
    {
      uint32_t vertNum = 0;
      uint32_t indexNum = 0;
      getNumVerticesAndIndicesFromGLTFMesh(gltfVehModel, mesh, vertNum, indexNum);
      maxVertexCountPerMesh    = std::max(vertNum, maxVertexCountPerMesh);
      maxPrimitiveCountPerMesh = std::max(indexNum / 3, maxPrimitiveCountPerMesh);
      totalVerticesCount      += vertNum;
      totalPrimitiveCount     += indexNum / 3;
      totalMeshes++;
    }

    maxVertexCountPerMesh    = std::max((uint32_t)8, maxVertexCountPerMesh);
    maxPrimitiveCountPerMesh = std::max((uint32_t)12, maxPrimitiveCountPerMesh); // this just to be working 

    InitGeoBuffersGPU(totalMeshes, totalVerticesCount, totalPrimitiveCount * 3);
    if(m_config.build_acc_structs)
    {
      m_pBuilderV2->Init(maxVertexCountPerMesh, maxPrimitiveCountPerMesh, totalPrimitiveCount,
        m_pMeshData->SingleVertexSize(), m_config.build_acc_structs_while_loading_scene);
    }

    std::unordered_map<int, uint32_t> loaded_meshes_to_meshId;
    for(size_t i = 0; i < scene.nodes.size(); ++i)
    {
      const tinygltf::Node node = gltfModel.nodes[scene.nodes[i]];
      auto identity = LiteMath::float4x4();
      LoadGLTFNodesRecursive(gltfModel, node, identity, loaded_meshes_to_meshId, false);
    }
    
    //load vehicle here
    //meshCounter = loaded_meshes_to_meshId.size();
    std::unordered_map<int, uint32_t> vehloaded_meshes_to_meshId;
    const tinygltf::Scene& scene = gltfVehModel.scenes[0];
    for(size_t i = 0; i < scene.nodes.size(); ++i)
    {
      const tinygltf::Node node = gltfVehModel.nodes[scene.nodes[0]];
      auto identity = m_currVehicleInstanceMatrices[0];
      LoadGLTFNodesRecursive(gltfVehModel, node, identity, vehloaded_meshes_to_meshId, true);
    }
  }

  if(m_config.load_materials != MATERIAL_LOAD_MODE::NONE)
  {
    m_materials.reserve(m_materials.size());
    for(const tinygltf::Material &gltfMat : gltfModel.materials)
    {
      MaterialData_pbrMR mat = materialDataFromGLTF(gltfMat);
      m_materials.push_back(mat);
    }

    m_dynMaterials.reserve(gltfVehModel.materials.size());
    for(const tinygltf::Material &gltfMat : gltfVehModel.materials)
    {
      MaterialData_pbrMR mat = materialDataFromGLTF(gltfMat);
      m_dynMaterials.push_back(mat);
    }
  }

  if(m_config.load_materials == MATERIAL_LOAD_MODE::MATERIALS_AND_TEXTURES)
  {
    m_textureInfos.reserve(m_materials.size() * 4);
    for (tinygltf::Image &image : gltfModel.images)
    {
      auto texturePath      = sceneFolder + image.uri;
      ImageFileInfo texInfo = getImageInfo(texturePath);
      if(!texInfo.is_ok)
      {
        std::stringstream ss;
        ss << "Texture at \"" << texturePath << "\" is absent or corrupted." ;
        vk_utils::logWarning(ss.str());
      }
      m_textureInfos.push_back(texInfo);
    }
    m_dynTextureInfos.reserve(gltfVehModel.materials.size() * 4);
    for (tinygltf::Image &image : gltfVehModel.images)
    {
      auto texturePath      = vehicleFolder + image.uri;
      ImageFileInfo texInfo = getImageInfo(texturePath);
      if(!texInfo.is_ok)
      {
        std::stringstream ss;
        ss << "Texture at \"" << texturePath << "\" is absent or corrupted." ;
        vk_utils::logWarning(ss.str());
      }
      m_dynTextureInfos.push_back(texInfo);
    }
  }
  
  if(m_config.load_geometry)
  {
    LoadCommonGeoDataOnGPU();
  }

  if(m_config.instance_matrix_as_vertex_attribute | m_config.instance_matrix_as_storage_buffer)
  {
    LoadInstanceDataOnGPU();
  }

  if(m_config.load_materials != MATERIAL_LOAD_MODE::NONE)
  {
    LoadMaterialDataOnGPU();
  }


  return true;
}

void SceneManager::LoadGLTFNodesRecursive(const tinygltf::Model &a_model, const tinygltf::Node& a_node, const LiteMath::float4x4& a_parentMatrix,
  std::unordered_map<int, uint32_t> &a_loadedMeshesToMeshId, bool loadVehicle)
{
  auto nodeMatrix = a_parentMatrix * transformMatrixFromGLTFNode(a_node);
  
  if(m_config.debug_output)
  {
    std::cout << "Node Name # " << a_node.name << std::endl;
    for (size_t i = 0; i < a_node.children.size(); i++)
    {
      std::cout << "Child Name " << a_model.nodes[a_node.children[i]].name << std::endl;
    }
    std::cout <<"---"<< std::endl;
  }
    

  for (size_t i = 0; i < a_node.children.size(); i++)
  {
    LoadGLTFNodesRecursive(a_model, a_model.nodes[a_node.children[i]], nodeMatrix, a_loadedMeshesToMeshId, loadVehicle);
  }

  if(a_node.mesh > -1)
  {
    int meshMapId = a_node.mesh;
    if(!a_loadedMeshesToMeshId.count(meshMapId))
    {
      const tinygltf::Mesh mesh = a_model.meshes[a_node.mesh];
      auto simpleMesh           = simpleMeshFromGLTFMesh(a_model, mesh);

      if(simpleMesh.VerticesNum() > 0)
      {
        auto meshId                         = AddMeshFromData(simpleMesh, loadVehicle);
        a_loadedMeshesToMeshId[meshMapId] = meshId;

        if(m_config.debug_output)
          std::cout << "Loading mesh # " << meshId << "Name " << mesh.name << std::endl;

        LoadOneMeshOnGPU(meshId, loadVehicle);

        if(m_config.build_acc_structs)
        {
          AddBLAS(meshId);
        }
      }
    }
    if (loadVehicle)
    {
      InstanceMesh(a_loadedMeshesToMeshId[a_node.mesh], nodeMatrix, true, true);
    }
    else
    {
      InstanceMesh(a_loadedMeshesToMeshId[a_node.mesh], nodeMatrix);
    }
  }
}