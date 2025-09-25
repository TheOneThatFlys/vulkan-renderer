#pragma once

#include <bitset>
#include <queue>
#include <unordered_map>
#include <cassert>
#include <ranges>
#include <memory>
#include <unordered_set>

#include "Common.h"

// shamelessly 'inspired' by https://austinmorlan.com/posts/entity_component_system

namespace ECS {

    using Entity = i32;
    static constexpr Entity MAX_ENTITIES = 4096;

    using ComponentID = u8;
    static constexpr ComponentID MAX_COMPONENTS = 64;

    using Signature = std::bitset<MAX_COMPONENTS>;

    constexpr Entity NULL_ENTITY = -1;

    class IComponentArray {
    public:
        virtual ~IComponentArray() = default;
        virtual void entityDestroyed(Entity entity) = 0;
    };
    // Stores components of type T in a densely packed vector
    template<typename T>
    class ComponentArray : public IComponentArray {
    public:
        void addComponent(Entity entity, T component) {
            assert(!m_entityToIndex.contains(entity));
            u32 index = static_cast<u32>(m_components.size());
            m_indexToEntity[index] = entity;
            m_entityToIndex[entity] = index;
            m_components.push_back(component);
        }

        void removeComponent(Entity entity) {
            assert(m_entityToIndex.contains(entity));

            // move last item into deleted slot to maintain packing
            u32 indexRemoved = m_entityToIndex[entity];
            u32 indexLast = static_cast<u32>(m_components.size() - 1);
            m_components[indexRemoved] = m_components[indexLast];

            // update maps
            Entity movedEntity = m_indexToEntity[indexLast];
            m_entityToIndex[movedEntity] = indexRemoved;
            m_indexToEntity[indexRemoved] = movedEntity;

            // erase last slot (deleted)
            m_entityToIndex.erase(entity);
            m_indexToEntity.erase(indexLast);

            m_components.pop_back();
        }

        T& getData(Entity entity) {
            return m_components[m_entityToIndex[entity]];
        }

        void entityDestroyed(Entity entity) override {
            if (m_entityToIndex.contains(entity))
                removeComponent(entity);
        }

    private:
        u32 m_last = 0;
        std::vector<T> m_components;
        std::unordered_map<Entity, u32> m_entityToIndex;
        std::unordered_map<u32, Entity> m_indexToEntity;
    };

    class ComponentManager {
    public:
        template<typename T>
        void registerComponent() {
            const char* typeName = typeid(T).name();
            assert(!m_componentIDs.contains(typeName));
            ComponentID id = m_componentCounter++;
            m_componentIDs[typeName] = id;
            m_componentArrays.insert({id, std::make_unique<ComponentArray<T>>()});
        }

        template<typename T>
        ComponentID getComponentID() const {
            return m_componentIDs.at(typeid(T).name());
        }

        template<typename T>
        T& getComponent(Entity entity) {
            return getComponentArray<T>()->getData(entity);
        }

        template<typename T>
        void addComponent(Entity entity, T component) {
            getComponentArray<T>()->addComponent(entity, component);
        }

        template<typename T>
        void removeComponent(Entity entity) {
            getComponentArray<T>()->removeComponent(entity);
        }

        void entityDestroyed(const Entity entity) {
            for (const auto& component : m_componentArrays | std::views::values) {
                component->entityDestroyed(entity);
            }
        }

    private:
        template<typename T>
        ComponentArray<T>* getComponentArray() {
            return static_cast<ComponentArray<T>*>(m_componentArrays.at(getComponentID<T>()).get());
        }

        std::unordered_map<const char*, ComponentID> m_componentIDs;
        std::unordered_map<ComponentID, std::unique_ptr<IComponentArray>> m_componentArrays;
        ComponentID m_componentCounter = 0;
    };

    class System {
    public:
        virtual ~System() = default;
        virtual void onEntityAdd(const Entity entity) {
            m_entities.insert(entity);
        }
        virtual void onEntityRemove(const Entity entity) {
            m_entities.erase(entity);
        }
        const std::unordered_set<Entity>& getEntities() {
            return m_entities;
        }
        bool contains(const Entity entity) const {
            return m_entities.contains(entity);
        }
    protected:
        std::unordered_set<Entity> m_entities;
    };

    class SystemManager {
    public:
        template<typename T, typename... Args>
        T* registerSystem(Args&&... args) {
            const char* key = typeid(T).name();
            assert(!m_systems.contains(key));
            m_systems.insert({key, std::make_unique<T>(args...)});
            // return static_cast<T*>(m_systems.at(key).get());
            return getSystem<T>();
        }

        template<typename T>
        T* getSystem() {
            const char* type = typeid(T).name();
            return static_cast<T*>(m_systems.at(type).get());
        }

        template<typename T>
        void setSignature(Signature signature) {
            const char* type = typeid(T).name();
            m_signatures.insert({type, signature});
        }

        void entityDestroyed(const Entity entity) {
            for (const auto& system : m_systems | std::views::values) {
                if (system->contains(entity)) {
                    system->onEntityRemove(entity);
                }
            }
        }

        void entitySignatureChanged(const Entity entity, const Signature entitySignature) {
            for (const auto& [type, system] : m_systems) {
                const Signature& systemSignature = m_signatures[type];
                if ((systemSignature & entitySignature) == systemSignature) {
                    if (!system->contains(entity)) {
                        system->onEntityAdd(entity);
                    }
                }
                else {
                    if (system->contains(entity)) {
                        system->onEntityRemove(entity);
                    }
                }
            }
        }

    private:
        std::unordered_map<const char*, Signature> m_signatures;
        std::unordered_map<const char*, std::unique_ptr<System>> m_systems;
    };

    class EntityManager {
    public:
        EntityManager() {
            for (Entity e = 0; e < MAX_ENTITIES; ++e) {
                m_availableIds.push(e);
            }
        }

        Entity createEntity() {
            assert(m_entitiesCount < MAX_ENTITIES);
            // take an id from the queue
            const Entity id = m_availableIds.front();
            m_availableIds.pop();
            // increase number of entities alive
            ++m_entitiesCount;
            return id;
        }

        void destroyEntity(Entity entity) {
            assert(entity < MAX_ENTITIES);
            m_signatures.at(entity).reset();
            m_availableIds.push(entity);
            --m_entitiesCount;
        }

        void setSignature(Entity entity, Signature signature) {
            assert(entity < MAX_ENTITIES);
            m_signatures.at(entity) = signature;
        }

        Signature getSignature(Entity entity) const {
            assert(entity < MAX_ENTITIES);

            return m_signatures.at(entity);
        }
    private:

        std::queue<Entity> m_availableIds;
        u32 m_entitiesCount = 0;
        std::array<Signature, MAX_ENTITIES> m_signatures;
    };

    inline std::unique_ptr<EntityManager> g_entityManager;
    inline std::unique_ptr<SystemManager> g_systemManager;
    inline std::unique_ptr<ComponentManager> g_componentManager;

    inline void init() {
        g_entityManager = std::make_unique<EntityManager>();
        g_systemManager = std::make_unique<SystemManager>();
        g_componentManager = std::make_unique<ComponentManager>();
    }

    inline void destroy() {
        g_entityManager = nullptr;
        g_systemManager = nullptr;
        g_componentManager = nullptr;
    }

    inline void destroyEntity(Entity entity) {
        g_entityManager->destroyEntity(entity);
        g_componentManager->entityDestroyed(entity);
        g_systemManager->entityDestroyed(entity);
    }

    template<typename T>
    void registerComponent() {
        g_componentManager->registerComponent<T>();
    }

    template<typename T>
    void addComponent(Entity entity, T component) {
        g_componentManager->addComponent<T>(entity, component);
        Signature signature = g_entityManager->getSignature(entity);
        signature.set(g_componentManager->getComponentID<T>(), true);
        g_entityManager->setSignature(entity, signature);
        g_systemManager->entitySignatureChanged(entity, signature);

    }

    inline Entity createEntity() {
        return g_entityManager->createEntity();
    }

    template<typename T>
    void removeComponent(Entity entity) {
        g_componentManager->removeComponent<T>(entity);
        Signature signature = g_entityManager->getSignature(entity);
        signature.set(g_componentManager->getComponentID<T>(), false);
        g_entityManager->setSignature(entity, signature);
        g_systemManager->entitySignatureChanged(entity, signature);
    }

    inline Signature getSignature(Entity entity) {
        return g_entityManager->getSignature(entity);
    }

    template<typename T>
    bool hasComponent(Entity entity) {
        return g_entityManager->getSignature(entity).test(g_componentManager->getComponentID<T>());
    }

    template<typename T>
    T& getComponent(Entity entity) {
        assert(hasComponent<T>(entity));
        return g_componentManager->getComponent<T>(entity);
    }

    template<typename T>
    T* getComponentOptional(Entity entity) {
        T* ret = nullptr;
        if (hasComponent<T>(entity)) {
            ret = &getComponent<T>(entity);
        }
        return ret;
    }

    template<typename T>
    ComponentID getComponentID() {
        return g_componentManager->getComponentID<T>();
    }

    template<typename T, typename... Args>
    T* registerSystem(Args&&... args) {
        return g_systemManager->registerSystem<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    T* getSystem() {
        return g_systemManager->getSystem<T>();
    }

    template<typename T>
    void setSystemSignature(Signature signature) {
        g_systemManager->setSignature<T>(signature);
    }

    template<typename... Ts>
    Signature createSignature() {
        Signature signature;
        (signature.set(getComponentID<Ts>()), ...);
        return signature;
    }

}

