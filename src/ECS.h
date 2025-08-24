#pragma once

#include <bitset>
#include <queue>
#include <unordered_map>
#include <cassert>
#include <ranges>
#include <set>

#include "Common.h"

namespace ECS {

    using Entity = u32;
    static constexpr Entity MAX_ENTITIES = 1024;

    using ComponentID = u8;
    static constexpr ComponentID MAX_COMPONENTS = 64;

    using Signature = std::bitset<MAX_COMPONENTS>;

    class IComponentArray {
    public:
        virtual ~IComponentArray() = default;
        virtual void entityDestroyed(Entity entity) = 0;
    };
    // Stores components of type T in a densely packed array
    template<typename T>
    class ComponentArray : IComponentArray {
    public:
        void addComponent(Entity entity, T component) {
            assert(!m_entityToIndex.contains(entity));
            u32 index = m_last;
            m_indexToEntity[index] = entity;
            m_entityToIndex[entity] = index;
            ++m_last;
        }

        void removeComponent(Entity entity) {
            assert(m_entityToIndex.contains(entity));

            // move last item into deleted slot to maintain packing
            u32 indexRemoved = m_entityToIndex[entity];
            u32 indexLast = m_last - 1;
            m_components[indexRemoved] = m_components[indexLast];

            // update maps
            Entity movedEntity = m_indexToEntity[indexLast];
            m_entityToIndex[movedEntity] = indexRemoved;
            m_indexToEntity[indexRemoved] = movedEntity;

            // erase last slot (deleted)
            m_entityToIndex.erase(entity);
            m_indexToEntity.erase(indexLast);

            // decrement last pointer
            --m_last;
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
        std::array<T, MAX_ENTITIES> m_components;
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
            m_componentArrays[id] = std::make_unique<ComponentArray<T>>();
        }

        template<typename T>
        ComponentID getComponentID() const {
            return m_componentIDs.at(typeid(T).name());
        }

        template<typename T>
        T& getComponent(Entity entity) const {
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

        void entityDestroyed(Entity entity) {
            for (const auto& component : m_componentArrays | std::views::values) {
                component->entityDestroyed(entity);
            }
        }

    private:
        template<typename T>
        ComponentArray<T>* getComponentArray() {
            return static_cast<ComponentArray<T>*>(m_componentArrays.at(getComponentID<T>()));
        }

        std::unordered_map<const char*, ComponentID> m_componentIDs;
        std::unordered_map<ComponentID, std::unique_ptr<IComponentArray>> m_componentArrays;
        ComponentID m_componentCounter = 0;
    };

    class System {
    public:
        std::set<Entity> entities;
    };

    class SystemManager {
    public:
        template<typename T>
        T* registerSystem() {
            const char* key = typeid(T).name();
            assert(!m_systems.contains(key));
            m_systems[key] = std::make_unique<T>();
            return m_systems.at(key).get();
        }

        template<typename T>
        void setSignature(Signature signature) {
            const char* type = typeid(T).name();
            m_signatures[type] = signature;
        }

        void entityDestroyed(Entity entity) {
            for (const auto& system : m_systems | std::views::values) {
                system->entities.erase(entity);
            }
        }

        void entitySignatureChanged(Entity entity, Signature entitySignature) {
            for (const auto& [type, system] : m_systems) {
                const Signature& systemSignature = m_signatures[type];
                if ((systemSignature & entitySignature) == systemSignature) {
                    system->entities.insert(entity);
                }
                else {
                    system->entities.erase(entity);
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
            Entity id = m_availableIds.front();
            m_availableIds.pop();
            // increase number of entities alive
            ++m_entitiesCount;
            return id;
        }

        void destroyEntity(Entity entity) {
            assert(entity < MAX_ENTITIES);
            m_signatures[entity].reset();
            m_availableIds.push(entity);
            --m_entitiesCount;
        }

        void setSignature(Entity entity, Signature signature) {
            assert(entity < MAX_ENTITIES);
            m_signatures[entity] = signature;
        }

        Signature getSignature(Entity entity) {
            assert(entity < MAX_ENTITIES);
            return m_signatures[entity];
        }
    private:

        std::queue<Entity> m_availableIds;
        u32 m_entitiesCount = 0;
        std::array<Signature, MAX_COMPONENTS> m_signatures;
    };

    inline std::unique_ptr<EntityManager> g_entityManager;
    inline std::unique_ptr<SystemManager> g_systemManager;
    inline std::unique_ptr<ComponentManager> g_componentManager;

    inline void init() {
        g_entityManager = std::make_unique<EntityManager>();
        g_systemManager = std::make_unique<SystemManager>();
        g_componentManager = std::make_unique<ComponentManager>();
    }

    inline Entity createEntity() {
        return g_entityManager->createEntity();
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

    template<typename T>
    void removeComponent(Entity entity) {
        g_componentManager->removeComponent<T>(entity);
        Signature signature = g_entityManager->getSignature(entity);
        signature.set(g_componentManager->getComponentID<T>(), false);
        g_entityManager->setSignature(entity, signature);
        g_systemManager->entitySignatureChanged(entity, signature);
    }

    template<typename T>
    T& getComponent(Entity entity) {
        return g_componentManager->getComponent<T>(entity);
    }

    template<typename T>
    ComponentID getComponentID() {
        return g_componentManager->getComponentID<T>();
    }

    template<typename T>
    T* registerSystem() {
        return g_systemManager->registerSystem<T>();
    }

    template<typename T>
    void setSystemSignature(Signature signature) {
        g_systemManager->setSignature<T>(signature);
    }

}

