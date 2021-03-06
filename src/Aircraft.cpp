#include "Aircraft.hpp"
#include "DataTables.hpp"
#include "ResourceHolder.hpp"
#include "Pickup.hpp"
#include "Utility.hpp"
#include "CommandQueue.hpp"
#include "SoundNode.hpp"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderStates.hpp>

#include <string>
#include <cmath>
#include <iostream>

// anonymous namespace to make the Table vector only usable in this file (this to avoid name collisions in other files)
namespace
{
	const std::vector<AircraftData> Table = initializeAircraftData();
}

Aircraft::Aircraft(Type type, const TextureHolder& textures, const FontHolder& fonts)
: Entity(Table[static_cast<unsigned int>(type)].hitpoints)
, mType(type)
, mSprite(textures.get(Table[static_cast<unsigned int>(type)].texture), Table[static_cast<unsigned int>(type)].textureRect)
, mExplosion(textures.get(TextureID::Explosion))
, mFireCommand()
, mMissileCommand()
, mDropPickupCommand()
, mFireCountdown(sf::Time::Zero)
, mIsFiring(false)
, mIsLaunchingMissile(false)
, mShowExplosion(true)
, mSpawnedPickup(false)
, mPlayedExplosionSound(false)
, mFireRateLevel(1)
, mSpreadLevel(1)
, mMissileAmmo(2)
, mTravelledDistance(0.f)
, mDirectionIndex(0)
, mHealthDisplay(nullptr)
, mMissileDisplay(nullptr)
, mScoreCounted(false)
{
	mExplosion.setFrameSize(sf::Vector2i(256, 256));
	mExplosion.setNumFrames(16);
	mExplosion.setDuration(sf::seconds(1));

	centerOrigin(mSprite);
	centerOrigin(mExplosion);

	mFireCommand.category = static_cast<unsigned int>(Category::Scene);
	mFireCommand.action = [this, &textures] (SceneNode& node, sf::Time)
	{
		createBullets(node, textures);
	};

	mMissileCommand.category = static_cast<unsigned int>(Category::Scene);
	mMissileCommand.action = [this, &textures] (SceneNode& node, sf::Time)
	{
		createProjectile(node, Projectile::Missile, 0.f, 0.5f, textures);
	};

	mDropPickupCommand.category = static_cast<unsigned int>(Category::Scene);
	mDropPickupCommand.action = [this, &textures] (SceneNode& node, sf::Time)
	{
		createPickup(node, textures);
	};

	auto healthDisplay = std::make_unique<TextNode>(fonts, "");
	mHealthDisplay = healthDisplay.get();
	attachChild(std::move(healthDisplay));

	if (getCategory() == static_cast<unsigned int>(Category::PlayerAircraft))
	{
		auto missileDisplay = std::make_unique<TextNode>(fonts, "");
		missileDisplay->setPosition(0, 70);
		mMissileDisplay = missileDisplay.get();
		attachChild(std::move(missileDisplay));
	}

	updateTexts();
}

unsigned int Aircraft::getCategory() const
{
	if (isAllied())
	{
		return static_cast<unsigned int>(Category::PlayerAircraft);
	}
	else
	{
		return static_cast<unsigned int>(Category::EnemyAircraft);
	}
}

sf::FloatRect Aircraft::getBoundingRect() const
{
	return getWorldTransform().transformRect(mSprite.getGlobalBounds());
}

bool Aircraft::isAllied() const
{
	return mType == Type::Eagle;
}

float Aircraft::getSpeed() const
{
	return Table[static_cast<unsigned int>(mType)].speed;
}

void Aircraft::increaseFireRate()
{
	if (mFireRateLevel < 10)
	{
		++mFireRateLevel;
	}
}

void Aircraft::increaseSpread()
{
	if (mSpreadLevel < 3)
	{
		++mSpreadLevel;
	}
}

void Aircraft::collectMissiles(unsigned int count)
{
	mMissileAmmo += count;
}

void Aircraft::fire()
{
	if (Table[static_cast<unsigned int>(mType)].fireInterval != sf::Time::Zero)
	{
		mIsFiring = true;
	}
}

void Aircraft::launchMissile()
{
	if (mMissileAmmo > 0)
	{
		mIsLaunchingMissile = true;
		--mMissileAmmo;
	}
}

void Aircraft::drawCurrent(sf::RenderTarget& target, sf::RenderStates states) const
{
	if (isDestroyed() && mShowExplosion)
	{
		target.draw(mExplosion, states);
	}
	else
	{
		target.draw(mSprite, states);
	}
}

void Aircraft::updateCurrent(sf::Time dt, CommandQueue& commands)
{
	updateTexts();
	updateRollAnimation();

	if (isDestroyed())
	{
		checkPickupDrop(commands);
		mExplosion.update(dt);

		// play explosion sound only once
		if (!mPlayedExplosionSound)
		{
			SoundEffectID soundEffect = (randomInt(2) == 0) ? SoundEffectID::Explosion1 : SoundEffectID::Explosion2;
			playLocalSound(commands, soundEffect);

			mPlayedExplosionSound = true;
		}

		return;
	}

	checkProjectileLaunch(dt, commands);

	updateMovementPattern(dt);
	Entity::updateCurrent(dt, commands);
}

void Aircraft::updateMovementPattern(sf::Time dt)
{
	// load movement pattern
	const std::vector<Direction>& directions = Table[static_cast<std::size_t>(mType)].directions;
	// enemy planes only
	if (!directions.empty())
	{
		// if moved far enough, change direction
		if (mTravelledDistance > directions[mDirectionIndex].distance)
		{
			mDirectionIndex = (mDirectionIndex + 1) % directions.size();
			mTravelledDistance = 0.f;
		}

		// compute velocity
		float radians = toRadian(directions[mDirectionIndex].angle + 90.f);
		float vx = getSpeed() * std::cos(radians);
		float vy = getSpeed() * std::sin(radians);
		setVelocity(vx, vy);
		mTravelledDistance += getSpeed() * dt.asSeconds();
	}
}

void Aircraft::checkPickupDrop(CommandQueue& commands)
{
	if (!isAllied() && randomInt(3) == 0 && !mSpawnedPickup)
	{
		commands.push(mDropPickupCommand);
	}

	mSpawnedPickup = true;
}

void Aircraft::checkProjectileLaunch(sf::Time dt, CommandQueue& commands)
{
	if (!isAllied())
	{
		fire();
	}

	if (mIsFiring && mFireCountdown <= sf::Time::Zero)
	{
		commands.push(mFireCommand);
		playLocalSound(commands, isAllied() ? SoundEffectID::AlliedGunfire : SoundEffectID::EnemyGunfire);

		mFireCountdown += Table[static_cast<unsigned int>(mType)].fireInterval / (mFireRateLevel + 1.f);
		mIsFiring = false;
	}
	else if (mFireCountdown > sf::Time::Zero)
	{
		mFireCountdown -= dt;
		mIsFiring = false;
	}

	if (mIsLaunchingMissile)
	{
		commands.push(mMissileCommand);
		playLocalSound(commands, SoundEffectID::LaunchMissile);
		mIsLaunchingMissile = false;
	}
}

void Aircraft::createBullets(SceneNode& node, const TextureHolder& textures) const
{
	Projectile::Type type = isAllied() ? Projectile::AlliedBullet : Projectile::EnemyBullet;

	switch (mSpreadLevel)
	{
		case 1:
			createProjectile(node, type, 0.0f, 0.5f, textures);
			break;

		case 2:
			createProjectile(node, type, -0.33f, 0.33f, textures);
			createProjectile(node, type, +0.33f, 0.33f, textures);
			break;

		case 3:
			createProjectile(node, type, -0.5f, 0.33f, textures);
			createProjectile(node, type,  0.0f, 0.5f, textures);
			createProjectile(node, type, +0.5f, 0.33f, textures);
			break;
	}
}

void Aircraft::createProjectile(SceneNode& node, Projectile::Type type, float xOffset, float yOffset, const TextureHolder& textures) const
{
	auto projectile = std::make_unique<Projectile>(type, textures);

	sf::Vector2f offset(xOffset * mSprite.getGlobalBounds().width, yOffset * mSprite.getGlobalBounds().height);
	sf::Vector2f velocity(0, projectile->getSpeed());

	float sign = isAllied() ? -1.f : +1.f;
	projectile->setPosition(getWorldPosition() + offset * sign);
	projectile->setVelocity(velocity * sign);
	node.attachChild(std::move(projectile));
}

void Aircraft::createPickup(SceneNode& node, const TextureHolder& textures) const
{
	auto type = static_cast<Pickup::Type>(randomInt(static_cast<unsigned int>(Pickup::Type::TypeCount)));

	auto pickup = std::make_unique<Pickup>(type, textures);
	pickup->setPosition(getWorldPosition());

	sf::Vector2f velocity(0.f, 1.f);
	pickup->setVelocity(velocity);

	node.attachChild(std::move(pickup));
}

void Aircraft::updateTexts()
{
	if (isDestroyed())
	{
		mHealthDisplay->setString("", true);
	}
	else
	{
		mHealthDisplay->setString(std::to_string(getHitpoints()) + " HP", true);
	}
	mHealthDisplay->setPosition(0.f, 50.f);
	mHealthDisplay->setRotation(-getRotation());

	if (mMissileDisplay)
	{
		if (mMissileAmmo == 0 || isDestroyed())
		{
			mMissileDisplay->setString("", true);
		}
		else
		{
			mMissileDisplay->setString("M: " + std::to_string(mMissileAmmo), true);
		}
	}
}

bool Aircraft::isMarkedForRemoval() const
{
	return isDestroyed() && (mExplosion.isFinished() || !mShowExplosion);
}

void Aircraft::remove()
{
	Entity::remove();
	mShowExplosion = false;
}

void Aircraft::updateRollAnimation()
{
	if (Table[static_cast<unsigned int>(mType)].hasRollAnimation)
	{
		sf::IntRect textureRect = Table[static_cast<unsigned int>(mType)].textureRect;

		if (getVelocity().x < 0.f)
		{
			textureRect.left += textureRect.width;
		}
		else if (getVelocity().x > 0.f)
		{
			textureRect.left += 2 * textureRect.width;
		}

		mSprite.setTextureRect(textureRect);
	}
}

void Aircraft::playLocalSound(CommandQueue& commands, SoundEffectID effect)
{
	Command command;
	command.category = static_cast<unsigned int>(Category::SoundEffect);
	command.action = derivedAction<SoundNode>(
		[effect] (SoundNode& node, sf::Time)
		{
			node.playSound(effect);
		});

	commands.push(command);
}

int Aircraft::getScoreValue()
{
	mScoreCounted = true;
	return Table[static_cast<unsigned int>(mType)].scoreValue;
}

bool Aircraft::isScoreCounted() const
{
	return mScoreCounted;
}
