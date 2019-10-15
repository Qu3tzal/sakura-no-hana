#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include "kantan/kantan.hpp"

#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>
#include <utility>

#include <cstdlib>
#include <cmath>

enum Difficulty {EASY, NORMAL, HARD, JAPANESE};

/**
    Constants.
**/
float COMBO_MIN = 5.f;
float LIFE_POINTS = 5.f;
float BALL_VELOCITY = 300.f;
float SAKURA_VELOCITY = -BALL_VELOCITY;
int SUGOI_COMBO = 10;
float BALLS_INTERVAL = 750.f;
float PLAYER_SPEED = 500.f;
float SHOOT_INTERVAL = 250.f;
float AFFINITY_CHANGE_INTERVAL = 25;

/**
    Helpers.
**/
template<typename T>
std::string to_string(T x)
{
    std::stringstream ss;
    ss << x;
    return ss.str();
}

/**
    Events.
**/
/*
    Events type enum.
*/
enum EventType {ColoredBallShot = 1, EntityDeath, PlayerHit};

/*
    Shot colored ball event data.
*/
class ColoredBallShotData : public kantan::EventData
{
    public:
        ColoredBallShotData(sf::Color color, sf::Vector2f center)
            : color(color)
            , center(center)
        {}

        sf::Color color;
        sf::Vector2f center;
};

/*
    An entity died.
*/
class EntityDeathData : public kantan::EventData
{
    public:
        EntityDeathData(kantan::Entity* e) : entity(e)
        {}

        kantan::Entity* entity;
};

/**
    Components.
**/
/*
    Deletion marker component.
*/
class DeletionMarkerComponent : public kantan::Component
{
    public:
        DeletionMarkerComponent()
            : kantan::Component(std::string("DeletionMarker"))
            , toDelete(false)
        {}

        bool toDelete;
};

/*
    Hitbox component.
*/
class HitboxComponent : public kantan::Component
{
    public:
        HitboxComponent()
            : kantan::Component(std::string("Hitbox"))
            , isBlocking(true)
        {}

        sf::FloatRect hitbox;
        bool isBlocking;
};

/*
    Sprite component.
*/
class SpriteComponent : public kantan::Component
{
    public:
        SpriteComponent() : kantan::Component(std::string("Sprite"))
        {}

        sf::Sprite sprite;
};

/*
    Movement component.
*/
class MovementComponent : public kantan::Component
{
    public:
        MovementComponent()
            : kantan::Component(std::string("Movement"))
        {}

        sf::Vector2f velocity;
};

/*
    Animation component.
*/
class AnimationComponent : public kantan::Component
{
    public:
        AnimationComponent()
            : kantan::Component(std::string("Animation"))
            , currentFrame(0)
            , lastFrame(sf::Time::Zero)
        {}

        // The frames subrects.
        std::vector<sf::IntRect> frames;

        // Current frame index.
        unsigned int currentFrame;

        // Time since last frame.
        sf::Time lastFrame;

        // Number of frame per seconds.
        unsigned int fps;
};

/*
    Life component.
*/
class LifeComponent : public kantan::Component
{
    public:
        LifeComponent()
            : kantan::Component(std::string("Life"))
            , lifepoints(LIFE_POINTS)
            , alive(true)
        {}

        int lifepoints;
        bool alive;
};

/*
    Particle component.
*/
class ParticleComponent : public kantan::Component
{
    public:
        ParticleComponent()
            : kantan::Component(std::string("Particle"))
            , m_particles(1000)
            , m_vertices(sf::Points, 1000)
        {}

        void init()
        {
            for(std::size_t i(0) ; i < m_particles.size() ; ++i)
            {
                m_vertices[i].color = color;
                m_vertices[i].position = center;
            }
        }

        sf::Color color;
        sf::Vector2f center;

        struct Particle
        {
            Particle()
                : lifetime(sf::seconds(1.f))
            {
                float angle = (std::rand() % 360) * 3.14f / 180.f;
                float speed = (std::rand() % 50) + 20.f;
                velocity = sf::Vector2f(std::cos(angle) * speed, std::sin(angle) * speed);
                lifetime = sf::milliseconds((std::rand() % 2000) + 1000);
            }

            sf::Vector2f velocity;
            sf::Time lifetime;
        };

        std::vector<Particle> m_particles;
        sf::VertexArray m_vertices;
        sf::Time lifetime;
};

/**
    Systems.
**/
/*
    Physic collision & response system.
*/
class PhysicSystem : public kantan::System
{
    public:
        PhysicSystem(){}

        // Update.
        virtual void update(sf::Time elapsed, std::vector<kantan::Entity*>& entities, std::queue<kantan::Event*>& eventQueue)
        {
            m_collisions.clear();

            // We check each entities against all the overs.
            // It's a naive and slow way of doing it.
            for(kantan::Entity* fst : entities)
            {
                // If one the entities has no hitbox, there cannot be a collision.
                // If this entity has no movement, it's not the one to modify.
                if(!fst->hasComponent("Hitbox") || !fst->hasComponent("Movement"))
                    continue;

                // We get the fst's hitbox & fst's movement.
                HitboxComponent* fstHitbox = fst->getComponent<HitboxComponent>("Hitbox");
                MovementComponent* fstMovement = fst->getComponent<MovementComponent>("Movement");

                for(kantan::Entity* snd : entities)
                {
                    // Do not check against yourself.
                    if(fst == snd)
                        continue;

                    // If one the entities has no hitbox, there cannot be a collision.
                    if(!snd->hasComponent("Hitbox"))
                        continue;

                    // Check if first entity has a movement component, if not we'll wait the turn of the second entity to check the collision.
                    if(!fst->hasComponent("Movement"))
                        continue;

                    // We get the snd's hitbox.
                    HitboxComponent* sndHitbox = snd->getComponent<HitboxComponent>("Hitbox");

                    // We copy the fst's hitbox and apply the movement to it.
                    sf::FloatRect fstNewHitbox = fstHitbox->hitbox;
                    fstNewHitbox.left += fstMovement->velocity.x * elapsed.asSeconds();
                    fstNewHitbox.top += fstMovement->velocity.y * elapsed.asSeconds();

                    // Effective movement, will be used to compute the corrected velocity.
                    sf::Vector2f movement(fstMovement->velocity.x * elapsed.asSeconds(), fstMovement->velocity.y * elapsed.asSeconds());

                    // Now we check the collision & compute the movement corrections.
                    if(fstNewHitbox.intersects(sndHitbox->hitbox))
                    {
                        // All checks are done relatively to the fst entity.
                        // To know from where the collision comes, we look at where the hitboxes were before the movement application.
                        // If the collision is from the bottom.
                        if(fstHitbox->hitbox.top + fstHitbox->hitbox.height <= sndHitbox->hitbox.top)
                        {
                            movement.y = sndHitbox->hitbox.top - (fstHitbox->hitbox.top + fstHitbox->hitbox.height);
                        }
                        // If the collision is from the top.
                        else if(fstHitbox->hitbox.top >= sndHitbox->hitbox.top + sndHitbox->hitbox.height)
                        {
                            movement.y = -(fstHitbox->hitbox.top - (sndHitbox->hitbox.top + sndHitbox->hitbox.height));
                        }
                        // If the collision is from the right.
                        else if(fstHitbox->hitbox.left + fstHitbox->hitbox.width <= sndHitbox->hitbox.left)
                        {
                            movement.x = sndHitbox->hitbox.left - (fstHitbox->hitbox.left + fstHitbox->hitbox.width);
                        }
                        // If the collision is from the left.
                        else if(fstHitbox->hitbox.left >= sndHitbox->hitbox.left + sndHitbox->hitbox.width)
                        {
                            movement.x = -(fstHitbox->hitbox.left - (sndHitbox->hitbox.left + sndHitbox->hitbox.width));
                        }
                        // Intern collision.
                        else
                        {
                            // We'll see later what to do here.
                        }

                        // Record the collision.
                        m_collisions.push_back(std::pair<kantan::Entity*, kantan::Entity*>(fst, snd));
                    }

                    // Change the velocity for the next entity check if both hitboxes are blocking.
                    if(fstHitbox->isBlocking && sndHitbox->isBlocking)
                    {
                        fstMovement->velocity.x = movement.x / elapsed.asSeconds();
                        fstMovement->velocity.y = movement.y / elapsed.asSeconds();
                    }
                }

                // Now we apply the corrected movement to the hitbox.
                fstHitbox->hitbox.left += fstMovement->velocity.x * elapsed.asSeconds();
                fstHitbox->hitbox.top += fstMovement->velocity.y * elapsed.asSeconds();
            }
        }

        // Returns the collisions record.
        std::vector<std::pair<kantan::Entity*, kantan::Entity*>> getCollisionRecord()
        {
            return m_collisions;
        }

    protected:
        // Record of the collisions.
        std::vector<std::pair<kantan::Entity*, kantan::Entity*>> m_collisions;
};

/*
    CollisionEffectsSystem.
    Check the collisions record.
*/
class CollisionEffectsSystem : public kantan::System
{
    public:
        CollisionEffectsSystem(){}

        // Sets the collision record.
        void setCollisionRecord(std::vector<std::pair<kantan::Entity*, kantan::Entity*>> collisions)
        {
            m_collisions = collisions;
        }

        // Updates.
        virtual void update(sf::Time elapsed, std::vector<kantan::Entity*>& entities, std::queue<kantan::Event*>& eventQueue)
        {
            for(std::pair<kantan::Entity*, kantan::Entity*> collision : m_collisions)
            {
                // When a sakura hits a ball, they die.
                if(collision.first->getName() == "Sakura" && collision.second->getName() == "Ball")
                {
                    collision.second->getComponent<LifeComponent>("Life")->lifepoints = 0;
                    collision.first->getComponent<LifeComponent>("Life")->lifepoints = 0;

                    DeletionMarkerComponent* dmc = collision.first->getComponent<DeletionMarkerComponent>("DeletionMarker");
                    dmc->toDelete = true;

                    dmc = collision.second->getComponent<DeletionMarkerComponent>("DeletionMarker");
                    dmc->toDelete = true;

                    // Create event.
                    kantan::Event* event = new kantan::Event(EventType::ColoredBallShot);

                    // Get ball color and center.
                    SpriteComponent* sprite = collision.second->getComponent<SpriteComponent>("Sprite");
                    sf::Color color;

                    if(sprite->sprite.getTextureRect().left < 64)
                        color = sf::Color::Red;
                    else if(sprite->sprite.getTextureRect().left < 64*2)
                        color = sf::Color::Blue;
                    else if(sprite->sprite.getTextureRect().left < 64*3)
                        color = sf::Color::Green;
                    else
                        color = sf::Color::Yellow;

                    sf::Vector2f center;
                    center.x = sprite->sprite.getGlobalBounds().left + sprite->sprite.getGlobalBounds().width / 2;
                    center.y = sprite->sprite.getGlobalBounds().top + sprite->sprite.getGlobalBounds().height / 2;

                    ColoredBallShotData* cbsd = new ColoredBallShotData(color, center);

                    // Attach data to event.
                    event->bindEventData(cbsd);

                    // Push event in queue.
                    eventQueue.push(event);
                }
                // When a ball hits a wall, it dies.
                else if(collision.first->getName() == "Ball" && collision.second->getName() == "Box")
                {
                    collision.first->getComponent<LifeComponent>("Life")->lifepoints = 0;

                    DeletionMarkerComponent* dmc = collision.first->getComponent<DeletionMarkerComponent>("DeletionMarker");
                    dmc->toDelete = true;
                }
                else if(collision.first->getName() == "Box" && collision.second->getName() == "Ball")
                {
                    collision.second->getComponent<LifeComponent>("Life")->lifepoints = 0;

                    DeletionMarkerComponent* dmc = collision.second->getComponent<DeletionMarkerComponent>("DeletionMarker");
                    dmc->toDelete = true;
                }
                // When a ball hits the player.
                else if(collision.first->getName() == "Ball" && collision.second->getName() == "Player")
                {
                    // Kill the ball.
                    collision.first->getComponent<LifeComponent>("Life")->lifepoints = 0;

                    DeletionMarkerComponent* dmc = collision.first->getComponent<DeletionMarkerComponent>("DeletionMarker");
                    dmc->toDelete = true;

                    // Decrease player's life.
                    LifeComponent* life = collision.second->getComponent<LifeComponent>("Life");
                    life->lifepoints--;

                    // Create event.
                    kantan::Event* event = new kantan::Event(EventType::PlayerHit);

                    // Push event in queue.
                    eventQueue.push(event);
                }
                else if(collision.first->getName() == "Player" && collision.second->getName() == "Ball")
                {
                    // Kill the ball.
                    collision.second->getComponent<LifeComponent>("Life")->lifepoints = 0;

                    DeletionMarkerComponent* dmc = collision.second->getComponent<DeletionMarkerComponent>("DeletionMarker");
                    dmc->toDelete = true;

                    // Decrease player's life.
                    LifeComponent* life = collision.first->getComponent<LifeComponent>("Life");
                    life->lifepoints--;

                    // Create event.
                    kantan::Event* event = new kantan::Event(EventType::PlayerHit);

                    // Push event in queue.
                    eventQueue.push(event);
                }
            }

            // Then clear.
            m_collisions.clear();
        }

    protected:
        // Collisions record.
        std::vector<std::pair<kantan::Entity*, kantan::Entity*>> m_collisions;
};

/*
    SynchronizeSystem.
    This system synchronize the rendering position with the hitbox position, if any.
*/
class SynchronizeSystem : public kantan::System
{
    public:
        SynchronizeSystem(){}

        // Update.
        virtual void update(sf::Time elapsed, std::vector<kantan::Entity*>& entities, std::queue<kantan::Event*>& eventQueue)
        {
            for(kantan::Entity* e : entities)
            {
                // If there is a hitbox and a sprite, we update the sprite. Otherwise we pass.
                if(!e->hasComponent("Hitbox") || !e->hasComponent("Sprite"))
                    continue;

                // Get hitbox and sprite.
                HitboxComponent* hitbox = e->getComponent<HitboxComponent>(std::string("Hitbox"));
                SpriteComponent* sprite = e->getComponent<SpriteComponent>(std::string("Sprite"));

                // Update sprite's position with hitbox's position.
                sprite->sprite.setPosition(hitbox->hitbox.left, hitbox->hitbox.top);
            }
        }
};

/*
    Animation System.
*/
class AnimationSystem : public kantan::System
{
    public:
        AnimationSystem(){}

        // Update.
        virtual void update(sf::Time elapsed, std::vector<kantan::Entity*>& entities, std::queue<kantan::Event*>& eventQueue)
        {
            for(kantan::Entity* e : entities)
            {
                // We need an animation and a sprite.
                if(!e->hasComponent("Sprite") || !e->hasComponent("Animation"))
                    continue;

                // Get the components.
                SpriteComponent* sprite = e->getComponent<SpriteComponent>("Sprite");
                AnimationComponent* animation = e->getComponent<AnimationComponent>("Animation");

                // Update time since last frame.
                animation->lastFrame += elapsed;

                // Check if we need to change frame.
                if(animation->lastFrame > sf::seconds(1.f/(animation->fps)))
                {
                    // Reset time since last frame.
                    animation->lastFrame = sf::Time::Zero;

                    // Change the frame index.
                    if(animation->currentFrame + 1 >= animation->frames.size())
                        animation->currentFrame = 0;
                    else
                        animation->currentFrame++;

                    // Get the next frame and apply it to the sprite.
                    sprite->sprite.setTextureRect(animation->frames[animation->currentFrame]);
                }
            }
        }
};

/*
    Sprite rendering system.
*/
class SpriteRenderSystem : public kantan::System
{
    public:
        SpriteRenderSystem(sf::RenderWindow* window) : m_window(window)
        {}

        // Update.
        virtual void update(sf::Time elapsed, std::vector<kantan::Entity*>& entities, std::queue<kantan::Event*>& eventQueue)
        {
            // View hitbox.
            sf::FloatRect viewHitbox(0.f, 0.f, m_window->getView().getSize().x, m_window->getView().getSize().y);

            for(kantan::Entity* e : entities)
            {
                // We need a sprite to render.
                if(!e->hasComponent("Sprite"))
                    continue;

                // Get the sprite component and render it.
                SpriteComponent* sprite = e->getComponent<SpriteComponent>("Sprite");

                if(viewHitbox.intersects(sprite->sprite.getGlobalBounds()))
                    m_window->draw(sprite->sprite);
            }
        }

    protected:
        // Window ptr.
        sf::RenderWindow* m_window;
};

/*
    Life system.
*/
class LifeSystem : public kantan::System
{
    public:
        LifeSystem(){}

        // Update.
        virtual void update(sf::Time elapsed, std::vector<kantan::Entity*>& entities, std::queue<kantan::Event*>& eventQueue)
        {
            for(kantan::Entity* e : entities)
            {
                // We need lifepoints.
                if(!e->hasComponent("Life"))
                    continue;

                // Get the life and check it.
                LifeComponent* life = e->getComponent<LifeComponent>("Life");

                // If no more lifepoints mark as dead and create an event.
                if(life->lifepoints <= 0)
                {
                    life->alive = false;

                    // Create event and data.
                    kantan::Event* event = new kantan::Event(EventType::EntityDeath);
                    EntityDeathData* death = new EntityDeathData(e);

                    // Bind them and push the event in the queue.
                    event->bindEventData(death);
                    eventQueue.push(event);
                }
            }
        }
};

/*
    Particle system updater.
*/
class ParticleWatcherSystem : public kantan::System
{
    public:
        ParticleWatcherSystem(){}

        // Update.
        virtual void update(sf::Time elapsed, std::vector<kantan::Entity*>& entities, std::queue<kantan::Event*>& eventQueue)
        {
            for(kantan::Entity* e : entities)
            {
                // We need a particle component.
                if(!e->hasComponent("Particle"))
                    continue;

                // Get the particles system.
                ParticleComponent* particles = e->getComponent<ParticleComponent>("Particle");

                // Get the lifetime of the overall system.
                particles->lifetime += elapsed;

                // Check if the system is outdated.
                if(particles->lifetime >= sf::seconds(2.f))
                {
                    // Ask for deletion.
                    DeletionMarkerComponent* dmc = e->getComponent<DeletionMarkerComponent>("DeletionMarker");
                    dmc->toDelete = true;

                    // No need to update the visual aspect.
                    continue;
                }

                // Update each individual particle of the particle system.
                for(std::size_t i = 0 ; i < particles->m_particles.size() ; ++i)
                {
                    // Update particle lifetime.
                    ParticleComponent::Particle& p = particles->m_particles[i];
                    p.lifetime -= elapsed;

                    // Update the position of the vertex corresponding.
                    particles->m_vertices[i].position += p.velocity * elapsed.asSeconds();

                    // Update alpha ratio.
                    float ratio = p.lifetime.asSeconds();
                    particles->m_vertices[i].color.a = static_cast<sf::Uint8>(ratio * 255);
                }
            }
        }

    protected:

};

/*
    Particle render system.
*/
class ParticleRenderSystem : public kantan::System
{
    public:
        ParticleRenderSystem(sf::RenderWindow* window) : m_window(window)
        {}

        // Update.
        virtual void update(sf::Time elapsed, std::vector<kantan::Entity*>& entities, std::queue<kantan::Event*>& eventQueue)
        {
            for(kantan::Entity* e : entities)
            {
                // We need a particle component to render.
                if(!e->hasComponent("Particle"))
                    continue;

                ParticleComponent* particles = e->getComponent<ParticleComponent>("Particle");
                m_window->draw(particles->m_vertices);
            }
        }

    protected:
        // Render window.
        sf::RenderWindow* m_window;
};

/**
    World.
**/
/*
    World class.
    Manage the entities.
*/
class World
{
    public:
        World(sf::RenderWindow* window, Difficulty difficulty)
            : m_window(window)
            , m_isRunning(true)
            , m_difficulty(difficulty)
            , m_lastMusic(0)
            , m_spriteRender(window)
            , m_particleRender(window)
            , m_colorAffinity(sf::Color::Red)
            , m_score(0)
            , m_combo(0)
            , m_lastSugoiDisplay(sf::seconds(1000.f))
            , m_lastAffinityChange(sf::Time::Zero)
        {
            switch(difficulty)
            {
                case Difficulty::EASY:
                    COMBO_MIN = 5.f;
                    LIFE_POINTS = 8.f;
                    BALL_VELOCITY = 300.f;
                    SAKURA_VELOCITY = -BALL_VELOCITY;
                    SUGOI_COMBO = 10;
                    BALLS_INTERVAL = 1000.f;
                    PLAYER_SPEED = 500.f;
                    SHOOT_INTERVAL = 250.f;
                    AFFINITY_CHANGE_INTERVAL = 30;
                    break;
                case Difficulty::NORMAL:
                    COMBO_MIN = 5.f;
                    LIFE_POINTS = 5.f;
                    BALL_VELOCITY = 300.f;
                    SAKURA_VELOCITY = -BALL_VELOCITY;
                    SUGOI_COMBO = 10;
                    BALLS_INTERVAL = 750.f;
                    PLAYER_SPEED = 500.f;
                    SHOOT_INTERVAL = 250.f;
                    AFFINITY_CHANGE_INTERVAL = 25;
                    break;
                case Difficulty::HARD:
                    COMBO_MIN = 10.f;
                    LIFE_POINTS = 3.f;
                    BALL_VELOCITY = 400.f;
                    SAKURA_VELOCITY = -BALL_VELOCITY;
                    SUGOI_COMBO = 20;
                    BALLS_INTERVAL = 250.f;
                    PLAYER_SPEED = 525.f;
                    SHOOT_INTERVAL = 225.f;
                    AFFINITY_CHANGE_INTERVAL = 15;
                    break;
                case Difficulty::JAPANESE:
                    COMBO_MIN = 20.f;
                    LIFE_POINTS = 1.f;
                    BALL_VELOCITY = 450.f;
                    SAKURA_VELOCITY = -BALL_VELOCITY;
                    SUGOI_COMBO = 50;
                    BALLS_INTERVAL = 150.f;
                    PLAYER_SPEED = 550.f;
                    SHOOT_INTERVAL = 200.f;
                    AFFINITY_CHANGE_INTERVAL = 5;
                    break;
            }
        }

        ~World()
        {
            for(unsigned int i(0) ; i < m_entities.size() ; ++i)
                delete m_entities[i];

            for(unsigned int i(0) ; i < m_components.size() ; ++i)
                delete m_components[i];
        }

        // Initialization.
        void init()
        {
            // Load assets.
            m_textures.load(0, "media/textures/smallboxAnimated.png");
            m_textures.load(1, "media/textures/littlesakura.png");
            m_textures.load(2, "media/textures/player.png");
            m_textures.load(3, "media/textures/balls.png");
            m_textures.load(4, "media/textures/heart.png");
            m_textures.load(5, "media/textures/sugoi.png");

            m_fonts.load(0, "media/fonts/OpenSans-Regular.ttf");

            m_sugoiSoundBuffer.loadFromFile("media/musics/sectionpass.wav");
            m_sugoiSound.setBuffer(m_sugoiSoundBuffer);

            m_hitSoundBuffer.loadFromFile("media/musics/Hollow_Hit_01.ogg");
            m_hitSound.setBuffer(m_hitSoundBuffer);

            m_changeAffinitySoundBuffer.loadFromFile("media/musics/Dark_Gleam.ogg");
            m_changeAffinitySound.setBuffer(m_changeAffinitySoundBuffer);

            m_hitGoodBallSoundBuffer.loadFromFile("media/musics/Comical_Pop_Sound.ogg");
            m_hitGoodBallSound.setBuffer(m_hitGoodBallSoundBuffer);

            m_hitWrongBallSoundBuffer.loadFromFile("media/musics/Awkward_Moment.ogg");
            m_hitWrongBallSound.setBuffer(m_hitWrongBallSoundBuffer);

            m_firstMusic.openFromFile("media/musics/Japan Tour (Dance Mix).ogg");
            m_secondMusic.openFromFile("media/musics/garlagan - Ruupu.ogg");

            m_firstMusic.setVolume(50);
            m_secondMusic.setVolume(50);

            m_firstMusic.play();
            m_firstMusic.setLoop(false);
            m_secondMusic.setLoop(false);

            // Add player.
            addPlayer();

            // Add boxes for the walls.
            buildWalls();
        }

        // Main loop.
        void update(sf::Time dt)
        {
            // Music.
            updatePlaylist();

            // Do not take too big time step.
            if(dt.asSeconds() > 0.5f)
                dt = sf::seconds(0.5f);

            /// Update the timers.
            m_lastSakuraShoot += dt;
            m_lastBallSpawn += dt;
            m_lastSugoiDisplay += dt;
            m_lastAffinityChange += dt;

            /// Real-time input.
            MovementComponent* movement = m_player->getComponent<MovementComponent>("Movement");
            movement->velocity = sf::Vector2f(0.f, 0.f);

            // Shoot.
            if(sf::Keyboard::isKeyPressed(sf::Keyboard::Space) && m_lastSakuraShoot > sf::milliseconds(SHOOT_INTERVAL))
            {
                HitboxComponent* hitbox = m_player->getComponent<HitboxComponent>("Hitbox");
                shootSakura(sf::Vector2f(hitbox->hitbox.left + hitbox->hitbox.width / 2.f - m_textures.get(1).getSize().x / 2.f,
                                         hitbox->hitbox.top - hitbox->hitbox.height / 2.f - m_textures.get(1).getSize().y / 2.f));
                m_lastSakuraShoot = sf::Time::Zero;
            }

            // Move.
            if(sf::Keyboard::isKeyPressed(sf::Keyboard::Q))
                movement->velocity.x = -PLAYER_SPEED;
            else if(sf::Keyboard::isKeyPressed(sf::Keyboard::D))
                movement->velocity.x = PLAYER_SPEED;

            /// Gameplay logic.
            if(m_lastBallSpawn > sf::milliseconds(BALLS_INTERVAL))
            {
                createBall();
                m_lastBallSpawn = sf::Time::Zero;
            }

            /// Animations.
            m_animations.update(dt, m_entities, m_eventQueue);

            /// Physics logic.
            m_physics.update(dt, m_entities, m_eventQueue);
            m_synchronize.update(dt, m_entities, m_eventQueue);

            /// Collision effects.
            m_collider.setCollisionRecord(m_physics.getCollisionRecord());
            m_collider.update(dt, m_entities, m_eventQueue);

            m_lifes.update(dt, m_entities, m_eventQueue);

            /// Handle (gameplay) events.
            kantan::Event event(0);
            while(kantan::pollEvent(event, m_eventQueue))
            {
                switch(event.getEventType())
                {
                    case EventType::PlayerHit:
                        // Reset combo and play hit sound.
                        m_combo = 0;
                        m_hitSound.play();

                        // Check if dead.
                        {
                            if(m_player->getComponent<LifeComponent>("Life")->lifepoints <= 0)
                            {
                                m_isRunning = false;

                                m_sugoiSound.stop();
                                m_hitSound.stop();
                                m_changeAffinitySound.stop();
                                m_hitGoodBallSound.stop();
                                m_hitWrongBallSound.stop();
                                m_firstMusic.stop();
                                m_secondMusic.stop();
                            }
                        }
                        break;
                    case EventType::ColoredBallShot:
                        {
                            // Make an explosion.
                            ColoredBallShotData* cbsd = event.getEventData<ColoredBallShotData>();
                            createExplosion(cbsd->color, cbsd->center);

                            // Update score and combo.
                            if(cbsd->color == m_colorAffinity)
                            {
                                // Play sound.
                                m_hitGoodBallSound.play();

                                m_combo++;

                                // Every 10 combo, sugoi sound.
                                if(m_combo > COMBO_MIN && m_combo % SUGOI_COMBO == 0)
                                {
                                    m_sugoiSound.play();
                                    m_lastSugoiDisplay = sf::Time::Zero;
                                }

                                // If combo, then it's more points !
                                if(m_combo > COMBO_MIN)
                                    m_score += m_combo;
                                else
                                    m_score++;
                            }
                            else
                            {
                                // Play sound.
                                m_hitWrongBallSound.play();

                                m_score--;
                                m_combo = 0;
                            }
                        }
                        break;
                    case EventType::EntityDeath:
                        break;
                    case 0:
                    default:
                        break;
                }
            }

            /// Check affinity change.
            if(m_lastAffinityChange > sf::seconds(AFFINITY_CHANGE_INTERVAL))
            {
                // Change color.
                if(m_colorAffinity == sf::Color::Yellow)
                    m_colorAffinity = sf::Color::Red;
                else if(m_colorAffinity == sf::Color::Red)
                    m_colorAffinity = sf::Color::Blue;
                else if(m_colorAffinity == sf::Color::Blue)
                    m_colorAffinity = sf::Color::Green;
                else if(m_colorAffinity == sf::Color::Green)
                    m_colorAffinity = sf::Color::Yellow;

                // Sound.
                m_changeAffinitySound.play();

                // Reset timer.
                m_lastAffinityChange = sf::Time::Zero;
            }

            /// Update particles.
            m_particleWatcher.update(dt, m_entities, m_eventQueue);

            /// Clean all the entities.
            cleanEntities();
        }

        void render()
        {
            // Entities.
            m_particleRender.update(sf::Time::Zero, m_entities, m_eventQueue);
            m_spriteRender.update(sf::Time::Zero, m_entities, m_eventQueue);

            // GUI.
            renderPlayerLife();
            renderPlayerScore();
            renderPlayerCombo();
            renderColorAffinity();

            if(m_combo > COMBO_MIN && m_combo % SUGOI_COMBO == 0 && m_lastSugoiDisplay < sf::seconds(1.5f))
                renderSugoi();
        }

        int getScore()
        {
            return m_score;
        }

        bool isRunning()
        {
            return m_isRunning;
        }

    protected:
        // Remove all the entities and their components if they are marked as "to delete".
        void cleanEntities()
        {
            for(auto itr_e = m_entities.begin() ; itr_e != m_entities.end() ;)
            {
                // If we need to delete the entity.
                if((*itr_e)->getComponent<DeletionMarkerComponent>("DeletionMarker")->toDelete)
                {
                    std::unordered_map<std::string, kantan::Component*> components = (*itr_e)->getAllComponents();

                    // First, delete all its components.
                    for(auto c = components.begin() ; c != components.end() ; ++c)
                    {
                        auto itr_c = std::find(m_components.begin(), m_components.end(), c->second);

                        if(itr_c != m_components.end())
                            m_components.erase(itr_c);
                    }

                    // Then delete the entity.
                    m_entities.erase(itr_e);
                }
                else
                    itr_e++;
            }
        }

        // Push an entity in the entities vector and return a pointer to it.
        kantan::Entity* createEntity(std::string name)
        {
            kantan::Entity* e = new kantan::Entity(name);

            DeletionMarkerComponent* dmc = createComponent<DeletionMarkerComponent>();
            e->addComponent(dmc);

            m_entities.push_back(e);
            return e;
        }

        // Push an component in the components vector and return a pointer to it.
        template<typename T>
        T* createComponent()
        {
            T* c = new T();
            m_components.push_back(c);
            return c;
        }

        // Create a box.
        void createBox(sf::Vector2f position)
        {
            // Create entity & components.
            kantan::Entity* box = createEntity("Box");

            SpriteComponent* sprite = createComponent<SpriteComponent>();
            HitboxComponent* hitbox = createComponent<HitboxComponent>();
            AnimationComponent* animation = createComponent<AnimationComponent>();

            // Configure components.
            sprite->sprite.setTexture(m_textures.get(0));
            sprite->sprite.setTextureRect(sf::IntRect(0, 0, 64, 64));
            hitbox->hitbox = sf::FloatRect(position, sf::Vector2f(64.f, 64.f));

            for(unsigned int i(0) ; i < 18 ; ++i)
                animation->frames.push_back(sf::IntRect(64 * i, 0, 64, 64));
            for(unsigned int i(0) ; i < 18 ; ++i)
                animation->frames.push_back(sf::IntRect(64 * i, 64, 64, 64));
            animation->fps = 24;

            // Add components.
            box->addComponent(sprite);
            box->addComponent(hitbox);
            box->addComponent(animation);
        }

        // Build the 4 necessary walls.
        void buildWalls()
        {
            // Left wall.
            for(unsigned int i(0) ; i < 12 ; ++i)
                createBox(sf::Vector2f(0, 64 * i));

            // Bottom wall.
            for(unsigned int i(1) ; i < 11 ; ++i)
                createBox(sf::Vector2f(64 * i, 704));

            // Right wall.
            for(unsigned int i(0) ; i < 12 ; ++i)
                createBox(sf::Vector2f(704, 64 * i));
        }

        // Shot a sakura.
        void shootSakura(sf::Vector2f position)
        {
            // Create entity & components.
            kantan::Entity* sakura = createEntity("Sakura");

            SpriteComponent* sprite = createComponent<SpriteComponent>();
            HitboxComponent* hitbox = createComponent<HitboxComponent>();
            MovementComponent* movement = createComponent<MovementComponent>();
            LifeComponent* life = createComponent<LifeComponent>();

            // Configure components.
            sprite->sprite.setTexture(m_textures.get(1));
            hitbox->hitbox = sf::FloatRect(position, sf::Vector2f(sprite->sprite.getGlobalBounds().width, sprite->sprite.getGlobalBounds().height));
            hitbox->isBlocking = false;
            movement->velocity = sf::Vector2f(0.f, SAKURA_VELOCITY);
            life->lifepoints = 1;

            // Add components.
            sakura->addComponent(sprite);
            sakura->addComponent(hitbox);
            sakura->addComponent(movement);
            sakura->addComponent(life);
        }

        // Add the player.
        void addPlayer()
        {
            // Create entity & components.
            m_player = createEntity("Player");

            SpriteComponent* sprite = createComponent<SpriteComponent>();
            HitboxComponent* hitbox = createComponent<HitboxComponent>();
            MovementComponent* movement = createComponent<MovementComponent>();
            LifeComponent* life = createComponent<LifeComponent>();

            // Configure components.
            sprite->sprite.setTexture(m_textures.get(2));
            hitbox->hitbox = sf::FloatRect(sf::Vector2f(65, 640), sf::Vector2f(sprite->sprite.getGlobalBounds().width, sprite->sprite.getGlobalBounds().height));
            movement->velocity = sf::Vector2f(0.f, 0.f);
            life->lifepoints = LIFE_POINTS;

            // Add components.
            m_player->addComponent(sprite);
            m_player->addComponent(hitbox);
            m_player->addComponent(movement);
            m_player->addComponent(life);
        }

        // Create a ball.
        void createBall()
        {
            // Generate random x.
            int randomX = 65 + ((std::rand() * 1000) % 576);
            int randomColor = 64 * (rand() % (int)(3 + 1));

            // Create entity & components.
            kantan::Entity* box = createEntity("Ball");

            SpriteComponent* sprite = createComponent<SpriteComponent>();
            HitboxComponent* hitbox = createComponent<HitboxComponent>();
            MovementComponent* movement = createComponent<MovementComponent>();
            LifeComponent* life = createComponent<LifeComponent>();

            // Configure components.
            sprite->sprite.setTexture(m_textures.get(3));
            sprite->sprite.setTextureRect(sf::IntRect(randomColor, 0, 64, 64));
            hitbox->hitbox = sf::FloatRect(sf::Vector2f(randomX, -64.f), sf::Vector2f(64.f, 64.f));
            hitbox->isBlocking = false;
            movement->velocity = sf::Vector2f(0.f, BALL_VELOCITY);
            life->lifepoints = 1;

            // Add components.
            box->addComponent(sprite);
            box->addComponent(hitbox);
            box->addComponent(movement);
            box->addComponent(life);
        }

        void createExplosion(sf::Color color, sf::Vector2f position)
        {
            // Create entity & components.
            kantan::Entity* explosion = createEntity("Explosion");

            ParticleComponent* particles = createComponent<ParticleComponent>();

            // Configure components.
            particles->color = color;
            particles->center = position;
            particles->init();

            // Add components.
            explosion->addComponent(particles);
        }

        // Render the player's score.
        void renderPlayerScore()
        {
            sf::Text scoreText;
            scoreText.setFont(m_fonts.get(0));
            scoreText.setCharacterSize(48);
            scoreText.setString(std::string("Score:") + to_string(m_score));
            scoreText.setPosition(5.f, 5.f);

            sf::RectangleShape bg;
            bg.setSize(sf::Vector2f(scoreText.getGlobalBounds().width + 20.f, scoreText.getGlobalBounds().height + 20.f));
            bg.setPosition(scoreText.getPosition());
            bg.setFillColor(sf::Color(0, 0, 0, 120));

            m_window->draw(bg);
            m_window->draw(scoreText);
        }

        // Render the player's combo if any.
        void renderPlayerCombo()
        {
            sf::Text comboText;
            comboText.setFont(m_fonts.get(0));

            // If good combo, be special.
            if(m_combo > COMBO_MIN)
            {
                comboText.setCharacterSize(52);
                comboText.setFillColor(sf::Color::Yellow);
                comboText.setString(std::string("COMBO: +") + to_string(m_combo));
            }
            else
            {
                comboText.setCharacterSize(48);
                comboText.setString(std::string("Combo: ") + to_string(m_combo));
            }

            comboText.setPosition(5.f, 60.f);

            sf::RectangleShape bg;
            bg.setSize(sf::Vector2f(comboText.getGlobalBounds().width + 20.f, comboText.getGlobalBounds().height + 20.f));
            bg.setPosition(comboText.getPosition());
            bg.setFillColor(sf::Color(0, 0, 0, 120));

            m_window->draw(bg);
            m_window->draw(comboText);
        }

        // Render the current color affinity.
        void renderColorAffinity()
        {
            sf::Sprite affinity;
            affinity.setTexture(m_textures.get(3));

            if(m_colorAffinity == sf::Color::Red)
                affinity.setTextureRect(sf::IntRect(0, 0, 64, 64));
            else if(m_colorAffinity == sf::Color::Blue)
                affinity.setTextureRect(sf::IntRect(64, 0, 64, 64));
            else if(m_colorAffinity == sf::Color::Green)
                affinity.setTextureRect(sf::IntRect(64*2, 0, 64, 64));
            else if(m_colorAffinity == sf::Color::Yellow)
                affinity.setTextureRect(sf::IntRect(64*3, 0, 64, 64));

            affinity.setScale(1.5f, 1.5f);
            affinity.setPosition(m_window->getSize().x - affinity.getGlobalBounds().width - 20.f, 20.f);

            m_window->draw(affinity);
        }

        // Renders the hearths of the player's life.
        void renderPlayerLife()
        {
            // Prepare the sprite.
            sf::Sprite heart;
            heart.setTexture(m_textures.get(4));

            // Get the life.
            LifeComponent* life = m_player->getComponent<LifeComponent>("Life");

            // Draw.
            for(int i(0) ; i < life->lifepoints ; ++i)
            {
                heart.setPosition(20.f + i * 40.f, 720.f);

                m_window->draw(heart);
            }
        }

        // Make sure the music is always on !
        void updatePlaylist()
        {
            if(m_firstMusic.getStatus() != sf::Music::Playing && m_secondMusic.getStatus() != sf::Music::Playing && m_lastMusic == 0)
            {
                m_secondMusic.play();
                m_lastMusic = 1;
            }
            else if(m_firstMusic.getStatus() != sf::Music::Playing && m_secondMusic.getStatus() != sf::Music::Playing && m_lastMusic == 1)
            {
                m_firstMusic.play();
                m_lastMusic = 0;
            }
        }

        // WE NEED MORE SUGOI.
        void renderSugoi()
        {
            sf::Sprite sugoi;
            sugoi.setTexture(m_textures.get(5));
            sugoi.setOrigin(sugoi.getGlobalBounds().width / 2, sugoi.getGlobalBounds().height / 2);
            sugoi.setPosition(m_window->getSize().x / 2, m_window->getSize().y / 2);

            m_window->draw(sugoi);
        }

    protected:
        // Window ptr.
        sf::RenderWindow* m_window;
        bool m_isRunning;
        Difficulty m_difficulty;
        kantan::TextureHolder m_textures;
        kantan::FontHolder m_fonts;

        // Music and sound.
        sf::Music m_firstMusic, m_secondMusic;
        int m_lastMusic;
        sf::SoundBuffer m_sugoiSoundBuffer, m_hitSoundBuffer, m_changeAffinitySoundBuffer, m_hitGoodBallSoundBuffer, m_hitWrongBallSoundBuffer;
        sf::Sound m_sugoiSound, m_hitSound, m_changeAffinitySound, m_hitGoodBallSound, m_hitWrongBallSound;

        // Event queue.
        std::queue<kantan::Event*> m_eventQueue;

        // Systems.
        LifeSystem m_lifes;
        PhysicSystem m_physics;
        CollisionEffectsSystem m_collider;
        SynchronizeSystem m_synchronize;
        AnimationSystem m_animations;
        SpriteRenderSystem m_spriteRender;
        ParticleRenderSystem m_particleRender;
        ParticleWatcherSystem m_particleWatcher;

        // Entities vector.
        std::vector<kantan::Entity*> m_entities;

        // Components vector.
        std::vector<kantan::Component*> m_components;

        // Player.
        kantan::Entity* m_player;

        // The last sakura shoot.
        sf::Time m_lastSakuraShoot;

        // The last ball spawn.
        sf::Time m_lastBallSpawn;

        // Color affinity.
        sf::Color m_colorAffinity;

        // Score.
        int m_score;

        // Combo serie.
        int m_combo;

        // The last time sugoi has been displayed.
        sf::Time m_lastSugoiDisplay;

        // The last time we change affinity.
        sf::Time m_lastAffinityChange;
};

/**
    Menu.
**/
/*
	"Fake world to do a background for the menu" class.
*/
class MenuWorld
{
	public:
		MenuWorld(sf::RenderWindow* window)
		: m_window(window)
		, m_isRunning(true)
		, m_spriteRender(window)
		, m_particleRender(window)
		{
		}

		~MenuWorld()
		{
			for(unsigned int i(0) ; i < m_entities.size() ; ++i)
				delete m_entities[i];

			for(unsigned int i(0) ; i < m_components.size() ; ++i)
				delete m_components[i];
		}

		// Initialization.
		void init()
		{
			// Load assets.
			m_textures.load(0, "media/textures/smallboxAnimated.png");

			// Add boxes for the walls.
			buildWalls();
		}

		// Main loop.
		void update(sf::Time dt)
		{
			// Do not take too big time step.
			if(dt.asSeconds() > 0.5f)
				dt = sf::seconds(0.5f);

			/// Animations.
			m_animations.update(dt, m_entities, m_eventQueue);

			/// Update particles.
			m_particleWatcher.update(dt, m_entities, m_eventQueue);

			/// Clean all the entities.
			cleanEntities();
		}

		void render()
		{
			// Entities.
			m_particleRender.update(sf::Time::Zero, m_entities, m_eventQueue);
			m_spriteRender.update(sf::Time::Zero, m_entities, m_eventQueue);
		}

		bool isRunning()
		{
			return m_isRunning;
		}

	protected:
		// Remove all the entities and their components if they are marked as "to delete".
		void cleanEntities()
		{
			for(auto itr_e = m_entities.begin() ; itr_e != m_entities.end() ;)
			{
				// If we need to delete the entity.
				if((*itr_e)->getComponent<DeletionMarkerComponent>("DeletionMarker")->toDelete)
				{
					std::unordered_map<std::string, kantan::Component*> components = (*itr_e)->getAllComponents();

					// First, delete all its components.
					for(auto c = components.begin() ; c != components.end() ; ++c)
					{
						auto itr_c = std::find(m_components.begin(), m_components.end(), c->second);

						if(itr_c != m_components.end())
							m_components.erase(itr_c);
					}

					// Then delete the entity.
					m_entities.erase(itr_e);
				}
				else
					itr_e++;
			}
		}

		// Push an entity in the entities vector and return a pointer to it.
		kantan::Entity* createEntity(std::string name)
		{
			kantan::Entity* e = new kantan::Entity(name);

			DeletionMarkerComponent* dmc = createComponent<DeletionMarkerComponent>();
			e->addComponent(dmc);

			m_entities.push_back(e);
			return e;
		}

		// Push an component in the components vector and return a pointer to it.
		template<typename T>
		T* createComponent()
		{
			T* c = new T();
			m_components.push_back(c);
			return c;
		}

		// Create a box.
		void createBox(sf::Vector2f position)
		{
			// Create entity & components.
			kantan::Entity* box = createEntity("Box");

			SpriteComponent* sprite = createComponent<SpriteComponent>();
			AnimationComponent* animation = createComponent<AnimationComponent>();

			// Configure components.
			sprite->sprite.setTexture(m_textures.get(0));
			sprite->sprite.setTextureRect(sf::IntRect(0, 0, 64, 64));
			sprite->sprite.setPosition(position);

			for(unsigned int i(0) ; i < 18 ; ++i)
				animation->frames.push_back(sf::IntRect(64 * i, 0, 64, 64));
			for(unsigned int i(0) ; i < 18 ; ++i)
				animation->frames.push_back(sf::IntRect(64 * i, 64, 64, 64));
			animation->fps = 24;

			// Add components.
			box->addComponent(sprite);
			box->addComponent(animation);
		}

		// Build the 4 necessary walls.
		void buildWalls()
		{
			// Left wall.
			for(unsigned int i(0) ; i < 12 ; ++i)
				createBox(sf::Vector2f(0, 64 * i));

			// Bottom wall.
			for(unsigned int i(1) ; i < 11 ; ++i)
				createBox(sf::Vector2f(64 * i, 704));

			// Right wall.
			for(unsigned int i(0) ; i < 12 ; ++i)
				createBox(sf::Vector2f(704, 64 * i));

			// Top wall.
			for(unsigned int i(1) ; i < 11 ; ++i)
				createBox(sf::Vector2f(64 * i, 0));
		}

		void createExplosion(sf::Color color, sf::Vector2f position)
		{
			// Create entity & components.
			kantan::Entity* explosion = createEntity("Explosion");

			ParticleComponent* particles = createComponent<ParticleComponent>();

			// Configure components.
			particles->color = color;
			particles->center = position;
			particles->init();

			// Add components.
			explosion->addComponent(particles);
		}

	protected:
		// Window ptr.
		sf::RenderWindow* m_window;
		bool m_isRunning;

		kantan::TextureHolder m_textures;

		// Event queue.
		std::queue<kantan::Event*> m_eventQueue;

		// Systems.
		SynchronizeSystem m_synchronize;
		AnimationSystem m_animations;
		SpriteRenderSystem m_spriteRender;
		ParticleRenderSystem m_particleRender;
		ParticleWatcherSystem m_particleWatcher;

		// Entities vector.
		std::vector<kantan::Entity*> m_entities;

		// Components vector.
		std::vector<kantan::Component*> m_components;

		// Player.
		kantan::Entity* m_player;
};

/*
    Menu class.
*/
class Menu
{
    public:
        // Constructor.
        Menu(sf::RenderWindow &window)
            : menuDone(false)
            , chosenDifficulty(NORMAL)
            , bgWorld(&window)
        {
            font.loadFromFile("media/fonts/mplus-1m-regular.ttf");

            titleText.setFillColor(sf::Color(158, 104, 148));
	        titleText.setFont(font);
	        titleText.setCharacterSize(34);
	        titleText.setString(L"Sakura no Hana");
	        titleText.setOrigin((int)(titleText.getGlobalBounds().width / 2), (int)(titleText.getGlobalBounds().height / 2));
	        titleText.setPosition((int)(window.getSize().x / 2), 150.f);

	        subTitleText.setFillColor(sf::Color(158, 104, 148));
	        subTitleText.setFont(font);
	        subTitleText.setCharacterSize(34);
	        subTitleText.setString(L"桜の花");
	        subTitleText.setOrigin((int)(subTitleText.getGlobalBounds().width / 2), (int)(subTitleText.getGlobalBounds().height / 2));
	        subTitleText.setPosition((int)(window.getSize().x / 2), titleText.getGlobalBounds().top + titleText.getGlobalBounds().height + 20.f);

            editionText.setFillColor(sf::Color::Yellow);
            editionText.setOutlineThickness(1.f);
            editionText.setOutlineColor(sf::Color(42, 42, 42));
            editionText.setFont(font);
            editionText.setCharacterSize((int)(titleText.getCharacterSize() * 0.75f));
            editionText.setString("TOKYO EDITION");
            editionText.setOrigin((int)(editionText.getGlobalBounds().width / 2), (int)(editionText.getGlobalBounds().height / 2));
            editionText.setPosition(titleText.getGlobalBounds().left + titleText.getGlobalBounds().width, 150.f);
            editionText.setRotation(20.f);

	        easyDifficultyText.setFillColor(sf::Color(42, 42, 42));
	        easyDifficultyText.setFont(font);
	        easyDifficultyText.setString(L"Easy");
	        easyDifficultyText.setPosition(100.f, 250.f);

	        normalDifficultyText.setFillColor(sf::Color(42, 42, 42));
	        normalDifficultyText.setFont(font);
	        normalDifficultyText.setString(L"Normal");
	        normalDifficultyText.setPosition(100.f, 250.f + 75.f);

	        hardDifficultyText.setFillColor(sf::Color(42, 42, 42));
	        hardDifficultyText.setFont(font);
	        hardDifficultyText.setString(L"Hard");
	        hardDifficultyText.setPosition(100.f, 250.f + 2.f * 75.f);

	        japaneseDifficultyText.setFillColor(sf::Color(42, 42, 42));
	        japaneseDifficultyText.setFont(font);
	        japaneseDifficultyText.setString(L"Japanese");
	        japaneseDifficultyText.setPosition(100.f, 250.f + 3.f * 75.f);

	        quitText.setFillColor(sf::Color(42, 42, 42));
	        quitText.setFont(font);
	        quitText.setString(L"Quit");
	        quitText.setPosition(window.getSize().x - 100.f - quitText.getGlobalBounds().width, 250.f + 4.f * 75.f);

	        cursorTexture.loadFromFile("media/textures/littlesakura.png");
	        cursorSprite.setTexture(cursorTexture);
	        cursorSprite.setOrigin((int)(cursorSprite.getGlobalBounds().width / 2.f), (int)(cursorSprite.getGlobalBounds().height / 2.f));

	        bgWorld.init();
        }

        // Returns true if the player has chosen a difficulty.
        bool hasChosen() const
        {
            return menuDone;
        }

        // Returns the chosen difficulty.
        Difficulty getChosenDifficulty() const
        {
            return chosenDifficulty;
        }

        // Resets the menu.
        void reset()
        {
            menuDone = false;
            chosenDifficulty = NORMAL;
        }

        // Handles the event.
        void handleEvent(const sf::Event &event, sf::RenderWindow &window)
        {
            if (event.type == sf::Event::MouseButtonReleased)
            {
                menuDone = true;
                sf::Vector2f mousePos = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y));

                if (easyDifficultyText.getGlobalBounds().contains(mousePos))
                    chosenDifficulty = EASY;
                else if (normalDifficultyText.getGlobalBounds().contains(mousePos))
                    chosenDifficulty = NORMAL;
                else if (hardDifficultyText.getGlobalBounds().contains(mousePos))
                    chosenDifficulty = HARD;
                else if (japaneseDifficultyText.getGlobalBounds().contains(mousePos))
                    chosenDifficulty = JAPANESE;
                else if (quitText.getGlobalBounds().contains(mousePos))
                	window.close();
                else
                    menuDone = false;
            }
        }

        // Updates the menu.
        void update(sf::Time &dt, sf::RenderWindow &window)
        {
	        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
	        cursorSprite.setPosition(mousePos);

	        if (easyDifficultyText.getGlobalBounds().contains(mousePos))
	        {
	        	easyDifficultyText.setFillColor(sf::Color(158, 104, 148));
	        	easyDifficultyText.setCharacterSize(32);
	        }
	        else
            {
	        	easyDifficultyText.setFillColor(sf::Color(42, 42, 42));
	        	easyDifficultyText.setCharacterSize(30);
	        }

	        if (normalDifficultyText.getGlobalBounds().contains(mousePos))
	        {
		        normalDifficultyText.setFillColor(sf::Color(158, 104, 148));
		        normalDifficultyText.setCharacterSize(32);
	        }
	        else
	        {
		        normalDifficultyText.setFillColor(sf::Color(42, 42, 42));
		        normalDifficultyText.setCharacterSize(30);
	        }

	        if (hardDifficultyText.getGlobalBounds().contains(mousePos))
	        {
		        hardDifficultyText.setFillColor(sf::Color(158, 104, 148));
		        hardDifficultyText.setCharacterSize(32);
	        }
	        else
	        {
		        hardDifficultyText.setFillColor(sf::Color(42, 42, 42));
		        hardDifficultyText.setCharacterSize(30);
	        }

	        if (japaneseDifficultyText.getGlobalBounds().contains(mousePos))
	        {
		        japaneseDifficultyText.setFillColor(sf::Color(158, 104, 148));
		        japaneseDifficultyText.setCharacterSize(32);
	        }
	        else
	        {
		        japaneseDifficultyText.setFillColor(sf::Color(42, 42, 42));
		        japaneseDifficultyText.setCharacterSize(30);
	        }

	        if (quitText.getGlobalBounds().contains(mousePos))
	        {
		        quitText.setFillColor(sf::Color(158, 104, 148));
		        quitText.setCharacterSize(32);
	        }
	        else
	        {
		        quitText.setFillColor(sf::Color(42, 42, 42));
		        quitText.setCharacterSize(30);
	        }

	        bgWorld.update(dt);
        }

        // Renders the menu.
        void render(sf::RenderWindow &window)
        {
	        bgWorld.render();

            window.draw(titleText);
            window.draw(subTitleText);
            window.draw(editionText);

            window.draw(easyDifficultyText);
	        window.draw(normalDifficultyText);
	        window.draw(hardDifficultyText);
	        window.draw(japaneseDifficultyText);
	        window.draw(quitText);

	        window.draw(cursorSprite);
        }

    private:
        // Flag to check if a difficulty has been chosen.
        bool menuDone;

        // The chosen difficulty.
        Difficulty chosenDifficulty;

        // Font and texts.
        sf::Font font;
        sf::Text titleText, subTitleText, editionText,
                 easyDifficultyText, normalDifficultyText, hardDifficultyText, japaneseDifficultyText,
                 quitText;

        // Cursor sprite and texture.
        sf::Texture cursorTexture;
        sf::Sprite cursorSprite;

        // A background world.
        MenuWorld bgWorld;
};

/**
    Main.
**/
/*
    Entry point of the game.
    Executes the main loop and manages the states.
*/
int main()
{
    // Random seed.
    std::srand(std::time(NULL));

    // Window & game clock initialization.
    sf::RenderWindow window(sf::VideoMode(768, 768),
                            L"桜の花 1.1 TOKYO EDITION | Cherry Blossom - Let's go Japan ! Game Jam - Feb.07~08 2015");
    window.setMouseCursorVisible(false);

    sf::Clock gameclock;

    // Menu.
    Menu menu(window);

    do
    {
        while (window.isOpen() && !menu.hasChosen())
        {
            // Time management.
            sf::Time dt = gameclock.restart();

            // Event handling.
            sf::Event event;
            while (window.pollEvent(event))
            {
                // If [ESC] pressed or closing window.
                if (event.type == sf::Event::Closed
                    || (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape))
                    window.close();
                else
                    menu.handleEvent(event, window);
            }

            // Update the menu.
            menu.update(dt, window);

            // Render the world.
            window.clear(sf::Color::White);
            menu.render(window);
            window.display();
        }

        Difficulty difficulty = menu.getChosenDifficulty();
        menu.reset();

        // World initalization.
        World world(&window, difficulty);
        world.init();

        // Main loop.
        gameclock.restart();
        bool gameEnded = false;
        while (window.isOpen() && !gameEnded)
        {
            // Event handling.
            sf::Event event;
            while (window.pollEvent(event))
            {
                // If [ESC] pressed or closing window.
                if (event.type == sf::Event::Closed
                    || (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape))
                    window.close();
            }

            // Update the world.
            world.update(gameclock.restart());

            // Render the world.
            window.clear(sf::Color::White);
            world.render();
            window.display();

            // Check if game ended.
            if (!world.isRunning())
                gameEnded = true;
        }

        { // TODO: Need to get this in its own class ASAP.
            sf::Font font;
            font.loadFromFile("media/fonts/OpenSans-Regular.ttf");
            sf::Text scoreText;
            scoreText.setFillColor(sf::Color::Black);
            scoreText.setFont(font);
            scoreText.setString(std::string("Score: ") + to_string(world.getScore()) + std::string("\nPress any key to go back to the menu."));
            scoreText.setOrigin((int)(scoreText.getGlobalBounds().width / 2), (int)(scoreText.getGlobalBounds().height / 2));
            scoreText.setPosition((int)(window.getSize().x / 2), (int)(window.getSize().y / 2));

            sf::SoundBuffer buf;
            buf.loadFromFile("media/musics/Depression.ogg");
            sf::Sound s;
            s.setBuffer(buf);
            s.play();

            bool goBackToMenu = false;

            sf::Time scoreElapsedTime = sf::Time::Zero;

            while (window.isOpen() && !goBackToMenu)
            {
                scoreElapsedTime += gameclock.restart();

                // Event handling.
                sf::Event event;
                while (window.pollEvent(event))
                {
                    // If closing window.
                    if (event.type == sf::Event::Closed)
                        window.close();
                    else if (event.type == sf::Event::KeyPressed && scoreElapsedTime > sf::seconds(1.f))
                        goBackToMenu = true;
                }

                // Render the world.
                window.clear(sf::Color::White);
                window.draw(scoreText);
                window.display();
            }
        }
    } while (window.isOpen());

	return 0;
}
