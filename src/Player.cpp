#include "Player.hpp"
#include "CommandQueue.hpp"
#include "Aircraft.hpp"
#include "SceneNode.hpp"

#include <functional>

// functor for player aircraft movement
struct AircraftMover
{
	AircraftMover(float vx, float vy) : velocity(vx, vy)
	{}

	void operator() (Aircraft& aircraft, sf::Time) const
	{
		aircraft.accelerate(velocity * aircraft.getSpeed());
	}

	sf::Vector2f velocity;
};

Player::Player()
: mCurrentMissionStatus(MissionStatus::MissionRunning)
{
	initializeKeys();

	initializeActions();

	for (auto& pair : mActionBinding)
	{
		pair.second.category = static_cast<unsigned int>(Category::PlayerAircraft);
	}
}

void Player::initializeKeys()
{
	mKeyBinding[sf::Keyboard::Left] = Action::MoveLeft;
	mKeyBinding[sf::Keyboard::Right] = Action::MoveRight;
	mKeyBinding[sf::Keyboard::Up] = Action::MoveUp;
	mKeyBinding[sf::Keyboard::Down] = Action::MoveDown;
	mKeyBinding[sf::Keyboard::Space] = Action::Fire;
	mKeyBinding[sf::Keyboard::M] = Action::LaunchMissile;
}

void Player::initializeActions()
{
	mActionBinding[Action::MoveLeft].action = derivedAction<Aircraft>(AircraftMover(-1.f,  0.f));
	mActionBinding[Action::MoveRight].action = derivedAction<Aircraft>(AircraftMover(+1.f,  0.f));
	mActionBinding[Action::MoveUp].action = derivedAction<Aircraft>(AircraftMover( 0.f, -1.f));
	mActionBinding[Action::MoveDown].action = derivedAction<Aircraft>(AircraftMover( 0.f, +1.f));

	mActionBinding[Action::Fire].action = derivedAction<Aircraft>([] (Aircraft& a, sf::Time){ a.fire(); });
	mActionBinding[Action::LaunchMissile].action = derivedAction<Aircraft>([] (Aircraft& a, sf::Time){ a.launchMissile(); });
}

void Player::handleEvent(const sf::Event& event, CommandQueue& commandQueue)
{
	if (event.type == sf::Event::KeyPressed)
	{
		// check if pressed key appears in key bindings
		auto found = mKeyBinding.find(event.key.code);
		// if it does and it is not a real time action, trigger command
		if (found != mKeyBinding.end() && !isRealTimeAction(found->second))
		{
			commandQueue.push(mActionBinding[found->second]);
		}
	}
}

void Player::handleRealTimeInput(CommandQueue& commandQueue)
{
	for (auto pair : mKeyBinding)
	{
		if (sf::Keyboard::isKeyPressed(pair.first) && isRealTimeAction(pair.second))
		{
			commandQueue.push(mActionBinding[pair.second]);
		}
	}
}

bool Player::isRealTimeAction(Action action)
{
	switch (action)
	{
		case Action::MoveLeft:
		case Action::MoveRight:
		case Action::MoveUp:
		case Action::MoveDown:
		case Action::Fire:
			return true;
		default:
			return false;
	}
}

void Player::assignKey(Action action, sf::Keyboard::Key key)
{
	// remove all keys that already map to action
	for (auto itr = mKeyBinding.begin(); itr != mKeyBinding.end(); )
	{
		if (itr->second == action)
		{
			mKeyBinding.erase(itr++);
		}
		else
		{
			++itr;
		}
	}

	// insert new binding
	mKeyBinding[key] = action;
}

sf::Keyboard::Key Player::getAssignedKey(Action action) const
{
	for (auto pair : mKeyBinding)
	{
		if (pair.second == action)
		{
			return pair.first;
		}
	}

	return sf::Keyboard::Unknown;
}

void Player::setMissionStatus(MissionStatus status)
{
	mCurrentMissionStatus = status;
}

Player::MissionStatus Player::getMissionStatus() const
{
	return mCurrentMissionStatus;
}
