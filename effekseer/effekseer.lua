local M = {}

local effects_registry = {}

-- Регистрация пути к эффекту
function M.register(effect_id, path)
    effects_registry[effect_id] = path
end

-- Получение пути
function M.get_path(effect_id)
    return effects_registry[effect_id]
end

return M
