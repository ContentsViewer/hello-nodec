#ifndef PLAYER_CONTROL_SYSTEM_HPP_
#define PLAYER_CONTROL_SYSTEM_HPP_

#include "app.hpp"
#include "bullet_system.hpp"

struct PlayerControl {
    float speed{1.0f};
};

class SerializablePlayerControl : public nodec_scene_serialization::BaseSerializableComponent {
public:
    SerializablePlayerControl()
        : BaseSerializableComponent{this} {
    }

    float speed{1.0f};

    template<class Archive>
    void serialize(Archive &archive) {
        archive(cereal::make_nvp("speed", speed));
    }
};

CEREAL_REGISTER_TYPE(SerializablePlayerControl)
CEREAL_REGISTER_POLYMORPHIC_RELATION(nodec_scene_serialization::BaseSerializableComponent, SerializablePlayerControl)

class PlayerControlSystem {
public:
    PlayerControlSystem(nodec_world::World &world,
                        std::shared_ptr<nodec_input::keyboard::Keyboard> keyboard,
                        std::shared_ptr<nodec_input::mouse::Mouse> mouse,
                        nodec_scene_serialization::SceneSerialization &serialization) {
        using namespace nodec;
        using namespace nodec_scene;
        using namespace nodec_input::keyboard;
        using namespace nodec_input::mouse;

        world.stepped().connect([&](nodec_world::World &world) { on_stepped(world); });
        keyboard->key_event().connect([&](const nodec_input::keyboard::KeyEvent &event) {
            // logging::InfoStream(__FILE__, __LINE__) << event;
            if (event.key == Key::W) {
                w_pressed = (event.type == KeyEvent::Type::Press);
            }
            if (event.key == Key::A) {
                a_pressed = (event.type == KeyEvent::Type::Press);
            }
            if (event.key == Key::S) {
                s_pressed = (event.type == KeyEvent::Type::Press);
            }
            if (event.key == Key::D) {
                d_pressed = (event.type == KeyEvent::Type::Press);
            }
        });

        mouse->mouse_event().connect([&](const nodec_input::mouse::MouseEvent &event) {
            // logging::InfoStream(__FILE__, __LINE__) << event;
            static Vector2i prev_pos;

            if (event.type == MouseEvent::Type::Press && event.button == MouseButton::Right) {
                prev_pos = event.position;
            }
            if (event.type == MouseEvent::Type::Move && (event.buttons & MouseButton::Right)) {
                rotation_delta += event.position - prev_pos;
                prev_pos = event.position;
            }

            if (event.button & MouseButton::Left) {
                left_pressed_ = (event.type == MouseEvent::Type::Press);
            }
        });

        serialization.register_component<PlayerControl, SerializablePlayerControl>(
            [&](const PlayerControl &control) {
                auto serializable = std::make_unique<SerializablePlayerControl>();
                serializable->speed = control.speed;
                return serializable;
            },
            [&](const SerializablePlayerControl &serializable, SceneEntity entity, SceneRegistry &registry) {
                auto &control = registry.emplace_component<PlayerControl>(entity).first;
                control.speed = serializable.speed;
            });
    }

#ifdef EDITOR_MODE
    static void setup_editor(nodec_scene_editor::SceneEditor &editor) {
        editor.inspector_component_registry().register_component<PlayerControl>(
            "Player Control",
            [](PlayerControl &control) {
                ImGui::DragFloat("Speed", &control.speed);
            });
    }
#endif

private:
    void on_stepped(nodec_world::World &world) {
        using namespace nodec;
        using namespace nodec_scene;
        using namespace nodec_scene::components;
        using namespace nodec_rendering::components;
        using namespace nodec_scene_serialization::components;

        [&]() {
            if (!left_pressed_) return;
            if (world.clock().current_time() - prev_fire_time_ < 0.25f) return;

            prev_fire_time_ = world.clock().current_time();

            auto bullet_entt = world.scene().create_entity();
            world.scene().registry().emplace_component<Bullet>(bullet_entt);
            world.scene().registry().emplace_component<NonSerialized>(bullet_entt);

            logging::InfoStream(__FILE__, __LINE__) << "fire!";
        }();

        world.scene().registry().view<Transform, PlayerControl>().each([&](const SceneEntity &entity, Transform &trfm, PlayerControl &control) {
            const float delta_time = world.clock().delta_time();

            auto forward = math::gfx::rotate(Vector3f(0, 0, 1), trfm.local_rotation);
            auto right = math::gfx::rotate(Vector3f(1, 0, 0), trfm.local_rotation);

            Vector2f move_vec;

            if (w_pressed) move_vec.y += 1;
            if (s_pressed) move_vec.y -= 1;
            if (a_pressed) move_vec.x -= 1;
            if (d_pressed) move_vec.x += 1;
            if (math::norm(move_vec) > 0.001f) {
                move_vec = math::normalize(move_vec);

                trfm.local_position += move_vec.y * forward * control.speed * delta_time;
                trfm.local_position += move_vec.x * right * control.speed * delta_time;

                trfm.dirty = true;
            }

            if (math::norm(rotation_delta) > 1) {
                constexpr float SCALE_FACTOR = 0.1f;

                // Apply rotation around the local right vector after current rotation.
                trfm.local_rotation = math::gfx::quaternion_from_angle_axis(rotation_delta.y * SCALE_FACTOR, right) * trfm.local_rotation;

                // And apply rotation around the world up vector.
                trfm.local_rotation = math::gfx::quaternion_from_angle_axis(rotation_delta.x * SCALE_FACTOR, Vector3f(0.f, 1.f, 0.f)) * trfm.local_rotation;

                trfm.dirty = true;
                rotation_delta.set(0, 0);
            }
        });
    }

private:
    bool w_pressed{false};
    bool a_pressed{false};
    bool s_pressed{false};
    bool d_pressed{false};

    nodec::Vector2i rotation_delta;
    bool left_pressed_{false};
    float prev_fire_time_{0.0f};
};

#endif