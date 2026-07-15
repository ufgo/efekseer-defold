#pragma once

#include "Effekseer.h"
#include <dmsdk/sdk.h>

class TextureLoaderDefold : public Effekseer::TextureLoader
{
public:
    TextureLoaderDefold(Effekseer::TextureLoaderRef baseLoader);
    virtual ~TextureLoaderDefold();

    virtual Effekseer::TextureRef Load(const char16_t* path, Effekseer::TextureType textureType) override;
    virtual void Unload(Effekseer::TextureRef data) override;

private:
    Effekseer::TextureLoaderRef m_baseLoader;
};

class ModelLoaderDefold : public Effekseer::ModelLoader
{
public:
    ModelLoaderDefold(Effekseer::ModelLoaderRef baseLoader);
    virtual ~ModelLoaderDefold();

    virtual Effekseer::ModelRef Load(const char16_t* path) override;
    virtual void Unload(Effekseer::ModelRef data) override;

private:
    Effekseer::ModelLoaderRef m_baseLoader;
};

class MaterialLoaderDefold : public Effekseer::MaterialLoader
{
public:
    MaterialLoaderDefold(Effekseer::MaterialLoaderRef baseLoader);
    virtual ~MaterialLoaderDefold();

    virtual Effekseer::MaterialRef Load(const char16_t* path) override;
    virtual void Unload(Effekseer::MaterialRef data) override;

private:
    Effekseer::MaterialLoaderRef m_baseLoader;
};

class CurveLoaderDefold : public Effekseer::CurveLoader
{
public:
    CurveLoaderDefold(Effekseer::CurveLoaderRef baseLoader);
    virtual ~CurveLoaderDefold();

    virtual Effekseer::CurveRef Load(const char16_t* path) override;
    virtual void Unload(Effekseer::CurveRef data) override;

private:
    Effekseer::CurveLoaderRef m_baseLoader;
};
