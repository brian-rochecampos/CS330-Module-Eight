///////////////////////////////////////////////////////////////////////////////
// scenemanager.cpp
// ================
// This file contains the implementation of the `SceneManager` class, which is 
// responsible for managing the preparation and rendering of 3D scenes. It 
// handles textures, materials, lighting configurations, and object rendering.
//
// AUTHOR: Brian Battersby
// INSTITUTION: Southern New Hampshire University (SNHU)
// COURSE: CS-330 Computational Graphics and Visualization
//
// INITIAL VERSION: November 1, 2023
// LAST REVISED: December 1, 2024
//
// RESPONSIBILITIES:
// - Load, bind, and manage textures in OpenGL.
// - Define materials and lighting properties for 3D objects.
// - Manage transformations and shader configurations.
// - Render complex 3D scenes using basic meshes.
//
// NOTE: This implementation leverages external libraries like `stb_image` for 
// texture loading and GLM for matrix and vector operations.
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <iostream>
#include <chrono>
#include <cmath>
#include <cfloat>

// declare the global variables
namespace
{
    const char* g_ModelName = "model";
    const char* g_ColorValueName = "objectColor";
    const char* g_TextureValueName = "objectTexture";
    const char* g_UseTextureName = "bUseTexture";
    const char* g_UseLightingName = "bUseLighting";

    static const std::chrono::steady_clock::time_point g_StartTime = std::chrono::steady_clock::now();
}

/***********************************************************
 *  SceneManager()
 ***********************************************************/
SceneManager::SceneManager(ShaderManager* pShaderManager)
{
    m_pShaderManager = pShaderManager;
    m_basicMeshes = new ShapeMeshes();

    for (int i = 0; i < 16; i++)
    {
        m_textureIDs[i].tag = "/0";
        m_textureIDs[i].ID = -1;
    }
    m_loadedTextures = 0;
}

/***********************************************************
 *  ~SceneManager()
 ***********************************************************/
SceneManager::~SceneManager()
{
    m_pShaderManager = NULL;
    delete m_basicMeshes;
    m_basicMeshes = NULL;

    DestroyGLTextures();
}

/***********************************************************
 *  CreateGLTexture()
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
    int width = 0;
    int height = 0;
    int colorChannels = 0;
    GLuint textureID = 0;

    stbi_set_flip_vertically_on_load(true);

    unsigned char* image = stbi_load(filename, &width, &height, &colorChannels, 0);

    if (image)
    {
        std::cout << "Loaded texture: " << filename << " (" << width << "x" << height << ", channels: " << colorChannels << ")\n";

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (colorChannels == 3)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        else if (colorChannels == 4)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
        else
        {
            std::cout << "Unsupported channel count: " << colorChannels << std::endl;
            stbi_image_free(image);
            return false;
        }

        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(image);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_textureIDs[m_loadedTextures].ID = textureID;
        m_textureIDs[m_loadedTextures].tag = tag;
        m_loadedTextures++;

        return true;
    }

    std::cout << "Failed to load texture: " << filename << std::endl;
    return false;
}

/***********************************************************
 *  BindGLTextures()
 ***********************************************************/
void SceneManager::BindGLTextures()
{
    for (int i = 0; i < m_loadedTextures; i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
    }
}

/***********************************************************
 *  DestroyGLTextures()
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
    for (int i = 0; i < m_loadedTextures; i++)
    {
        glDeleteTextures(1, &m_textureIDs[i].ID);
    }
    m_loadedTextures = 0;
}


/***********************************************************
 *  FindTextureID()
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
    for (int i = 0; i < m_loadedTextures; i++)
    {
        if (m_textureIDs[i].tag == tag)
            return m_textureIDs[i].ID;
    }
    return -1;
}

/***********************************************************
 *  FindTextureSlot()
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
    for (int i = 0; i < m_loadedTextures; i++)
    {
        if (m_textureIDs[i].tag == tag)
            return i;
    }
    return -1;
}

/***********************************************************
 *  SetTransformations()
 ***********************************************************/
void SceneManager::SetTransformations(
    glm::vec3 scaleXYZ,
    float XrotationDegrees,
    float YrotationDegrees,
    float ZrotationDegrees,
    glm::vec3 positionXYZ)
{
    glm::mat4 modelView;

    glm::mat4 scale = glm::scale(scaleXYZ);
    glm::mat4 rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 translation = glm::translate(positionXYZ);

    modelView = translation * rotationZ * rotationY * rotationX * scale;

    if (m_pShaderManager)
    {
        m_pShaderManager->setMat4Value(g_ModelName, modelView);
    }
}

/***********************************************************
 *  SetShaderColor()
 ***********************************************************/
void SceneManager::SetShaderColor(
    float redColorValue,
    float greenColorValue,
    float blueColorValue,
    float alphaValue)
{
    glm::vec4 currentColor(redColorValue, greenColorValue, blueColorValue, alphaValue);

    if (m_pShaderManager)
    {
        m_pShaderManager->setIntValue(g_UseTextureName, false);
        m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
    }
}

/***********************************************************
 *  SetShaderTexture()
 ***********************************************************/
void SceneManager::SetShaderTexture(std::string textureTag)
{
    if (m_pShaderManager)
    {
        m_pShaderManager->setIntValue(g_UseTextureName, true);

        int textureSlot = FindTextureSlot(textureTag);
        if (textureSlot < 0) textureSlot = 0;
        m_pShaderManager->setSampler2DValue(g_TextureValueName, textureSlot);
    }
}

/***********************************************************
 *  SetTextureUVScale()
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
    if (m_pShaderManager)
    {
        m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
    }
}

/***********************************************************
 *  DefineObjectMaterials()
 ***********************************************************/
void SceneManager::DefineObjectMaterials()
{
    m_objectMaterials.clear();

    // Metal
    OBJECT_MATERIAL metal;
    metal.diffuseColor = glm::vec3(0.7f, 0.68f, 0.6f);
    metal.specularColor = glm::vec3(0.95f, 0.92f, 0.85f);
    metal.shininess = 64.0f;
    metal.tag = "metal";
    m_objectMaterials.push_back(metal);

    // Wood (table)
    OBJECT_MATERIAL wood;
    wood.diffuseColor = glm::vec3(0.45f, 0.3f, 0.15f);
    wood.specularColor = glm::vec3(0.05f, 0.05f, 0.05f);
    wood.shininess = 8.0f;
    wood.tag = "wood";
    m_objectMaterials.push_back(wood);

    // Candle (wax)
    OBJECT_MATERIAL candle;
    candle.diffuseColor = glm::vec3(0.95f, 0.92f, 0.85f);
    candle.specularColor = glm::vec3(0.2f, 0.2f, 0.2f);
    candle.shininess = 12.0f;
    candle.tag = "candle";
    m_objectMaterials.push_back(candle);

    // Flame (make relatively bright / slightly specular)
    OBJECT_MATERIAL flame;
    flame.diffuseColor = glm::vec3(1.0f, 0.7f, 0.25f);
    flame.specularColor = glm::vec3(0.9f, 0.6f, 0.2f);
    flame.shininess = 16.0f;
    flame.tag = "flame";
    m_objectMaterials.push_back(flame);

    // Cement / floor slight specular
    OBJECT_MATERIAL cement;
    cement.diffuseColor = glm::vec3(0.6f, 0.6f, 0.6f);
    cement.specularColor = glm::vec3(0.3f, 0.3f, 0.3f);
    cement.shininess = 16.0f;
    cement.tag = "cement";
    m_objectMaterials.push_back(cement);
}

/***********************************************************
 *  SetShaderMaterial()
 ***********************************************************/
void SceneManager::SetShaderMaterial(const std::string& materialTag)
{
    if (!m_pShaderManager) return;

    OBJECT_MATERIAL selected;
    bool found = false;
    for (auto& mat : m_objectMaterials)
    {
        if (mat.tag == materialTag)
        {
            selected = mat;
            found = true;
            break;
        }
    }

    if (!found)
    {
        selected.diffuseColor = glm::vec3(0.8f, 0.8f, 0.8f);
        selected.specularColor = glm::vec3(0.2f, 0.2f, 0.2f);
        selected.shininess = 8.0f;
    }

    m_pShaderManager->setVec3Value("material.diffuseColor", selected.diffuseColor);
    m_pShaderManager->setVec3Value("material.specularColor", selected.specularColor);
    m_pShaderManager->setFloatValue("material.shininess", selected.shininess);
}

/***********************************************************
 *  SetupSceneLights()
 ***********************************************************/
void SceneManager::SetupSceneLights()
{
    if (!m_pShaderManager) return;

    // Making sure the shader program is active
    m_pShaderManager->use();

    // Turn lighting on
    m_pShaderManager->setBoolValue("bUseLighting", true);

    // Directional light (soft top-down)
    m_pShaderManager->setVec3Value("directionalLight.direction", -0.2f, -1.0f, -0.3f);
    m_pShaderManager->setVec3Value("directionalLight.ambient", 0.12f, 0.12f, 0.12f);
    m_pShaderManager->setVec3Value("directionalLight.diffuse", 0.55f, 0.52f, 0.48f);
    m_pShaderManager->setVec3Value("directionalLight.specular", 0.4f, 0.4f, 0.4f);
    m_pShaderManager->setIntValue("directionalLight.bActive", true);

    // Point light 0 - warm candle light
    m_pShaderManager->setVec3Value("pointLights[0].position", 0.0f, 3.0f, 0.0f);
    m_pShaderManager->setVec3Value("pointLights[0].ambient", 0.06f, 0.03f, 0.02f);  // small warm ambient
    m_pShaderManager->setVec3Value("pointLights[0].diffuse", 0.95f, 0.6f, 0.25f);  // warm bright
    m_pShaderManager->setVec3Value("pointLights[0].specular", 1.0f, 0.8f, 0.5f);
    m_pShaderManager->setIntValue("pointLights[0].bActive", true);

    // Point light 1 - cool fill light to the left/back to avoid pure black shadows
    m_pShaderManager->setVec3Value("pointLights[1].position", -4.0f, 5.0f, -2.0f);
    m_pShaderManager->setVec3Value("pointLights[1].ambient", 0.03f, 0.03f, 0.05f);
    m_pShaderManager->setVec3Value("pointLights[1].diffuse", 0.35f, 0.45f, 0.6f);
    m_pShaderManager->setVec3Value("pointLights[1].specular", 0.35f, 0.35f, 0.4f);
    m_pShaderManager->setIntValue("pointLights[1].bActive", true);

    m_pShaderManager->setIntValue("spotLight.bActive", false);
}

/***********************************************************
 *  LoadSceneTextures()
 ***********************************************************/
void SceneManager::LoadSceneTextures()
{
    // loading all textures used in the scene
    CreateGLTexture("textures/wood.jpg", "wood");
    CreateGLTexture("textures/metal.jpg", "metal");
    CreateGLTexture("textures/candle.jpg", "candle");
    CreateGLTexture("textures/book.jpg", "book");
    CreateGLTexture("textures/page.jpg", "page");
    CreateGLTexture("textures/pen.jpg", "pen");
    CreateGLTexture("textures/inkpot.png", "inkpot");
    CreateGLTexture("textures/cloth.jpg", "cloth");

    BindGLTextures();
}

/***********************************************************
 *  PrepareScene()
 ***********************************************************/
void SceneManager::PrepareScene()
{
    LoadSceneTextures(); // loading all textures first

    m_basicMeshes->LoadBoxMesh();
    m_basicMeshes->LoadPlaneMesh();
    m_basicMeshes->LoadCylinderMesh(1.0f, 1.0f, 72);
    m_basicMeshes->LoadConeMesh();
    m_basicMeshes->LoadPrismMesh();
    m_basicMeshes->LoadPyramid4Mesh();
    m_basicMeshes->LoadSphereMesh();
    m_basicMeshes->LoadTaperedCylinderMesh();
    m_basicMeshes->LoadTorusMesh();

    DefineObjectMaterials();
    SetupSceneLights();
}

/***********************************************************
 *  RenderScene()
 ***********************************************************/
void SceneManager::RenderScene()
{
    glm::vec3 scaleXYZ;
    glm::vec3 positionXYZ;

    // background color
    glClearColor(0.74f, 0.72f, 0.70f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_pShaderManager)
    {
        m_pShaderManager->use();
        m_pShaderManager->setBoolValue("bUseLighting", true);
    }

    // ---------------------------
    // TABLE
    // ---------------------------
    scaleXYZ = glm::vec3(22.0f, 0.4f, 12.0f);
    positionXYZ = glm::vec3(0.0f, -0.3f, 0.0f);
    SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
    SetShaderTexture("wood");
    SetTextureUVScale(8.0f, 8.0f);
    SetShaderMaterial("cement");
    m_basicMeshes->DrawBoxMesh();

    // ---------------------------
    // CANDLE HOLDER + CANDLE
    // ---------------------------
    glm::vec3 candleOffset = glm::vec3(-3.5f, 0.0f, -3.0f);
    float currentY = 0.0f;

    // base of the candle holder
    scaleXYZ = glm::vec3(1.6f, 0.6f, 1.6f);
    positionXYZ = candleOffset + glm::vec3(0.0f, currentY, 0.0f);
    SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
    SetShaderTexture("metal");
    SetTextureUVScale(4.0f, 2.0f);
    m_basicMeshes->DrawTaperedCylinderMesh();
    currentY += 0.6f;

    // stem part
    scaleXYZ = glm::vec3(0.3f, 1.0f, 0.3f);
    positionXYZ = candleOffset + glm::vec3(0.0f, currentY, 0.0f);
    SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
    SetShaderTexture("metal");
    SetTextureUVScale(2.5f, 0.5f);
    m_basicMeshes->DrawCylinderMesh();
    currentY += 1.0f;

    // small metal sphere decoration
    scaleXYZ = glm::vec3(0.45f, 0.25f, 0.45f);
    positionXYZ = candleOffset + glm::vec3(0.0f, currentY, 0.0f);
    SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
    SetShaderTexture("metal");
    m_basicMeshes->DrawSphereMesh();
    currentY += 0.15f;

    // upper stem
    scaleXYZ = glm::vec3(0.3f, 0.8f, 0.3f);
    positionXYZ = candleOffset + glm::vec3(0.0f, currentY, 0.0f);
    SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
    SetShaderTexture("metal");
    m_basicMeshes->DrawCylinderMesh();
    currentY += 0.8f;

    // cup part
    scaleXYZ = glm::vec3(1.2f, 1.0f, 1.2f);
    positionXYZ = candleOffset + glm::vec3(0.0f, currentY + 0.7f, 0.0f);
    SetTransformations(scaleXYZ, 180, 0, 0, positionXYZ);
    SetShaderTexture("metal");
    m_basicMeshes->DrawTaperedCylinderMesh();

    // rim on top of the cup
    scaleXYZ = glm::vec3(1.2f, 0.2f, 1.2f);
    positionXYZ = candleOffset + glm::vec3(0.0f, currentY + 0.7f, 0.0f);
    SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
    SetShaderTexture("metal");
    m_basicMeshes->DrawCylinderMesh();
    currentY += 1.0f;

    // candle itself
    scaleXYZ = glm::vec3(0.9f, 2.0f, 0.9f);
    positionXYZ = candleOffset + glm::vec3(0.0f, currentY - 0.2f, 0.0f);
    SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
    SetShaderTexture("candle");
    SetTextureUVScale(1.0f, 0.8f);
    m_basicMeshes->DrawCylinderMesh();

    // wick
    scaleXYZ = glm::vec3(0.04f, 0.05f, 0.04f);
    positionXYZ = candleOffset + glm::vec3(0.0f, currentY + 1.8f, 0.0f);
    SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
    SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
    m_basicMeshes->DrawCylinderMesh();

    // candle light animation
    float elapsedSeconds = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - g_StartTime).count();
    float flicker = 0.92f + 0.12f * std::sin(elapsedSeconds * 12.0f)
        + 0.03f * std::sin(elapsedSeconds * 37.0f);

    glm::vec3 flamePos = candleOffset + glm::vec3(0.0f, currentY + 2.0f, 0.0f);
    if (m_pShaderManager)
    {
        m_pShaderManager->use();
        m_pShaderManager->setVec3Value("pointLights[0].position", flamePos);

        glm::vec3 baseDiffuse(0.95f, 0.60f, 0.25f);
        glm::vec3 baseAmbient(0.07f, 0.04f, 0.02f);

        glm::vec3 flickerDiffuse = baseDiffuse * flicker;
        glm::vec3 flickerAmbient = baseAmbient * (0.6f + 0.4f * flicker);

        m_pShaderManager->setVec3Value("pointLights[0].diffuse", flickerDiffuse);
        m_pShaderManager->setVec3Value("pointLights[0].ambient", flickerAmbient);
        m_pShaderManager->setVec3Value("pointLights[0].specular",
            1.0f * flicker, 0.8f * flicker, 0.5f * flicker);
        m_pShaderManager->setIntValue("pointLights[0].bActive", true);
    }

    // flame core
    scaleXYZ = glm::vec3(0.05f, 0.25f, 0.05f);
    SetTransformations(scaleXYZ, 0, 0, 0, flamePos);
    SetShaderColor(1.2f * flicker, 0.95f * flicker, 0.45f * flicker, 1.0f);
    m_basicMeshes->DrawSphereMesh();

    // glow around the flame
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    float glowPulse = 1.0f + 0.08f * std::sin(elapsedSeconds * 8.0f);
    scaleXYZ = glm::vec3(0.12f * glowPulse, 0.40f * glowPulse, 0.12f * glowPulse);
    positionXYZ = flamePos + glm::vec3(0.0f, 0.05f, 0.0f);
    SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
    SetShaderColor(1.0f, 0.9f, 0.7f, 0.3f * (0.9f + 0.1f * flicker));
    m_basicMeshes->DrawSphereMesh();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // book + pen + ink setup
    DrawBookSetup();
}


/***********************************************************
 *  DrawBookSetup() — My scene setup with book, pen, paper, and inkpot
 ***********************************************************/
void SceneManager::DrawBookSetup()
{
    glm::vec3 scaleXYZ, positionXYZ;

    // Tablecloth covering the whole table
    {
        glm::vec3 tableCenter = glm::vec3(0.0f, 0.0f, 0.0f);

        float clothWidth = 16.0f;
        float clothDepth = 10.0f;
        float clothThickness = 0.02f;

        glm::vec3 clothPos = tableCenter + glm::vec3(0.0f, -0.1f, 0.0f); // lifted a bit to stop flickering

        SetTransformations(glm::vec3(clothWidth, clothThickness, clothDepth),
            0.0f, 0.0f, 0.0f, clothPos);

        SetShaderTexture("cloth"); // using the tablecloth texture
        SetTextureUVScale(4.0f, 4.0f);
        m_basicMeshes->DrawBoxMesh();
    }

    // Main open book setup
    const glm::vec3 bookPosition = glm::vec3(-2.0f, 0.20f, 2.1f);
    const float bookScaleFactor = 1.4f;

    const float coverWidth = 4.6f * bookScaleFactor;
    const float coverDepth = 3.0f * bookScaleFactor;
    const float coverThickness = 0.25f * bookScaleFactor;
    const float pageWidth = 4.3f * bookScaleFactor;
    const float pageThickness = 0.025f * bookScaleFactor;
    const float baseRotationY = 4.5f; // small rotation to make it more natural

    // Bottom book cover
    scaleXYZ = glm::vec3(coverWidth, coverThickness * 0.95f, coverDepth);
    positionXYZ = bookPosition;
    SetTransformations(scaleXYZ, 0.0f, baseRotationY, 0.0f, positionXYZ);
    SetShaderTexture("book");
    SetTextureUVScale(2.0f, 1.5f);
    m_basicMeshes->DrawBoxMesh();

    // Book pages layered to look real
    const int numPageLayers = 25;
    float baseY = -0.02f * bookScaleFactor;
    for (int i = 0; i < numPageLayers; ++i)
    {
        float yOffset = baseY + i * (pageThickness * 0.8f);
        float subtleWave = 0.002f * sinf(i * 0.5f);

        float normalized = (i - numPageLayers / 2.0f) / (numPageLayers / 2.0f);
        float smoothCurve = powf(fabs(normalized), 1.5f);
        float archAmplitude = 0.10f * bookScaleFactor * (1.0f - smoothCurve);

        float xOffset = -0.01f * normalized;
        float pageYaw = normalized * 0.12f;

        float rotationAngleX = -archAmplitude * 0.5f;
        float rotationAngleY = baseRotationY + pageYaw;

        scaleXYZ = glm::vec3(pageWidth, pageThickness, coverDepth - 0.08f);
        positionXYZ = bookPosition + glm::vec3(xOffset, yOffset + subtleWave, 0.0f);

        SetTransformations(scaleXYZ, rotationAngleX, rotationAngleY, 0.0f, positionXYZ);
        SetShaderTexture("page");
        SetTextureUVScale(1.0f, 1.0f);
        m_basicMeshes->DrawBoxMesh();
    }

    // Center divider in the middle of the book
    {
        float totalHeight = numPageLayers * (pageThickness * 0.8f);
        float dividerCenterY = (-0.02f * bookScaleFactor) + (totalHeight * 0.5f);
        float dividerHeight = totalHeight * 1.05f;
        float dividerThickness = 0.05f * bookScaleFactor;
        float dividerDepth = coverDepth - 0.02f;

        scaleXYZ = glm::vec3(dividerThickness, dividerHeight, dividerDepth);
        positionXYZ = bookPosition + glm::vec3(0.0f, dividerCenterY, 0.0f);
        SetTransformations(scaleXYZ, 0.0f, baseRotationY, 0.0f, positionXYZ);
        SetShaderColor(0.11f, 0.09f, 0.08f, 1.0f);
        m_basicMeshes->DrawBoxMesh();

        // darker strip inside for detail
        scaleXYZ = glm::vec3(dividerThickness * 0.9f, dividerHeight * 0.95f, dividerDepth - 0.01f);
        positionXYZ = bookPosition + glm::vec3(0.0f, dividerCenterY - (pageThickness * 0.02f), 0.0f);
        SetTransformations(scaleXYZ, 0.0f, baseRotationY, 0.0f, positionXYZ);
        SetShaderColor(0.07f, 0.06f, 0.055f, 1.0f);
        m_basicMeshes->DrawBoxMesh();
    }

    // Pen next to the book
    {
        const float penScale = 1.7f;

        float length = 0.45f * bookScaleFactor * penScale;
        float rRear = 0.025f * bookScaleFactor * penScale;
        float rFront = 0.015f * bookScaleFactor * penScale;
        float tipLen = 0.06f * bookScaleFactor * penScale * 2.6f;
        float tipRadius = glm::max(0.0005f, rFront * 0.25f);

        float rotY = baseRotationY + 10.0f;
        float yawRad = glm::radians(rotY);
        glm::vec3 dir = glm::normalize(glm::vec3(sinf(yawRad), 0.0f, cosf(yawRad)));

        glm::vec3 center = bookPosition + glm::vec3(
            (coverWidth * 0.5f) + 0.85f,
            -0.20f + glm::max(rRear, rFront) + 0.002f,
            0.50f * bookScaleFactor
        );

        // pen body with texture
        if (m_pShaderManager) m_pShaderManager->setIntValue(g_UseTextureName, true);
        SetShaderTexture("pen");
        SetTextureUVScale(1.0f, 1.0f);
        SetTransformations(glm::vec3(rRear, rFront, length), 0.0f, rotY, 0.0f, center);
        m_basicMeshes->DrawTaperedCylinderMesh();

        // white pen tip
        glm::vec3 front = center + dir * (length * 0.5f + 0.003f);
        glm::vec3 tipPos = front + dir * (tipLen * 0.5f + 0.003f);
        glm::vec3 tipScale = glm::vec3(tipRadius, tipRadius, tipLen);

        if (m_pShaderManager) m_pShaderManager->setIntValue(g_UseTextureName, false);
        SetShaderColor(1.0f, 1.0f, 1.0f, 1.0f);
        SetTransformations(tipScale, 0.0f, rotY, 0.0f, tipPos);
        m_basicMeshes->DrawConeMesh();
    }

    // Inkpot next to the book
    const float inkPotScale = 1.5f;

    {
        glm::vec3 inkPotPos = bookPosition + glm::vec3(
            (coverWidth * 0.5f) + 0.95f,
            -0.30f,
            -2.8f * bookScaleFactor
        );

        if (m_pShaderManager) m_pShaderManager->setIntValue(g_UseTextureName, true);
        SetShaderTexture("inkpot");

        // inkpot base
        SetTransformations(glm::vec3(0.4f, 0.45f, 0.4f) * bookScaleFactor * inkPotScale,
            0, 0, 0,
            inkPotPos + glm::vec3(0.0f, 0.25f * bookScaleFactor * inkPotScale, 0.0f));
        m_basicMeshes->DrawSphereMesh();

        // inkpot neck
        SetTransformations(glm::vec3(0.18f, 0.2f, 0.18f) * bookScaleFactor * inkPotScale,
            0, 0, 0,
            inkPotPos + glm::vec3(0.0f, 0.5f * bookScaleFactor * inkPotScale, 0.0f));
        m_basicMeshes->DrawCylinderMesh();

        // lid on top
        if (m_pShaderManager) m_pShaderManager->setIntValue(g_UseTextureName, false);
        SetShaderColor(0.08f, 0.08f, 0.08f, 1.0f);
        SetTransformations(glm::vec3(0.22f, 0.08f, 0.22f) * bookScaleFactor * inkPotScale,
            0, 0, 0,
            inkPotPos + glm::vec3(0.0f, 0.6f * bookScaleFactor * inkPotScale, 0.0f));
        m_basicMeshes->DrawCylinderMesh();
    }

    // Paper under the book
    {
        glm::vec3 paperPos = bookPosition + glm::vec3(
            -0.04f,
            -0.27f,
            0.12f
        );

        float paperRotationY = baseRotationY + 8.0f;
        float paperScaleFactor = bookScaleFactor * 1.05f;

        glm::vec3 paperScale = glm::vec3(4.75f, 0.01f, 3.15f) * paperScaleFactor;

        if (m_pShaderManager) m_pShaderManager->setIntValue(g_UseTextureName, true);
        SetShaderTexture("page");
        SetTextureUVScale(1.5f, 1.5f);
        SetTransformations(paperScale, 0.0f, paperRotationY, 0.0f, paperPos);
        m_basicMeshes->DrawBoxMesh();
    }

    // Closed book near the corner of the table
    {
        glm::vec3 tableCenter = glm::vec3(0.0f, 0.0f, 0.0f);

        const float closedBookScale = 1.25f;
        const float bookRotY = 110.0f;

        const float coverWidth = 4.5f * closedBookScale;
        const float coverDepth = 3.0f * closedBookScale;
        const float coverThickness = 0.08f * closedBookScale;
        const float pagesHeight = 0.5f * closedBookScale;

        glm::vec3 closedBookPos = tableCenter + glm::vec3(6.0f, 0.1f, -1.8f);

        // bottom cover
        SetTransformations(glm::vec3(coverWidth, coverThickness, coverDepth),
            0.0f, bookRotY, 0.0f,
            closedBookPos);
        SetShaderTexture("book");
        SetTextureUVScale(2.2f, 1.8f);
        m_basicMeshes->DrawBoxMesh();

        // pages
        glm::vec3 pagePos = closedBookPos + glm::vec3(0.0f, coverThickness * 0.5f + pagesHeight * 0.5f, 0.0f);
        SetTransformations(glm::vec3(coverWidth * 0.96f, pagesHeight, coverDepth * 0.94f),
            0.0f, bookRotY, 0.0f,
            pagePos);
        SetShaderTexture("page");
        SetTextureUVScale(2.5f, 2.5f);
        m_basicMeshes->DrawBoxMesh();

        // spine on the left side
        {
            float spineThickness = 0.09f * closedBookScale;
            float spineHeight = pagesHeight + coverThickness + 0.03f;

            glm::vec3 localSpineOffset = glm::vec3(
                0.0f,
                coverThickness * 0.5f + pagesHeight * 0.5f,
                -coverDepth * 0.5f - (spineThickness * 0.5f) + 0.10f
            );

            float yawRad = glm::radians(bookRotY);
            glm::mat4 rotM = glm::rotate(glm::mat4(1.0f), yawRad, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::vec3 worldSpineOffset = glm::vec3(rotM * glm::vec4(localSpineOffset, 1.0f));

            glm::vec3 spinePos = closedBookPos + worldSpineOffset;

            SetTransformations(glm::vec3(coverWidth * 0.985f, spineHeight, spineThickness),
                0.0f, bookRotY, 0.0f,
                spinePos);

            SetShaderTexture("book");
            SetTextureUVScale(1.0f, 1.0f);
            m_basicMeshes->DrawBoxMesh();
        }

        // top cover
        glm::vec3 topCoverPos = closedBookPos + glm::vec3(0.0f, coverThickness + pagesHeight, 0.0f);
        SetTransformations(glm::vec3(coverWidth, coverThickness, coverDepth),
            0.0f, bookRotY, 0.0f,
            topCoverPos);
        SetShaderTexture("book");
        SetTextureUVScale(2.2f, 1.8f);
        m_basicMeshes->DrawBoxMesh();
    }
}
