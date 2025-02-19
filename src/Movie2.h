#pragma once

#include "Actions/Action.h"
#include "UIObject.h"
#ifndef USE_SFML_MOVIE_STUB
#include "PhysFSStream.h"
#include "sfeMovie/Movie.hpp"
#else
#include <SFML/Graphics/RectangleShape.hpp>
#endif

class Movie : public UIObject
{
private:
#ifndef USE_SFML_MOVIE_STUB
	sfe::Movie movie;
	std::unique_ptr<sf::PhysFSStream> file;
	sf::Vector2f size;
#else
	sf::RectangleShape movie;
#endif
	Anchor anchor{ Anchor::Top | Anchor::Left };
	bool visible{ true };
	std::shared_ptr<Action> actionComplete;

public:
#ifndef USE_SFML_MOVIE_STUB
	Movie(const std::string_view file_) : file(std::make_unique<sf::PhysFSStream>(file_.data())) {}

	bool load();
	void play() { movie.play(); }
	void pause() { movie.pause(); }
	void setVolume(float volume) { movie.setVolume(volume); }
#else
	Movie(const std::string_view file_) { movie.setFillColor(sf::Color::Black); }

	bool load();
	void play() noexcept {}
	void pause() noexcept {}
	void setVolume(float volume) noexcept {}
#endif

	virtual std::shared_ptr<Action> getAction(uint16_t nameHash16) const noexcept;
	virtual bool setAction(uint16_t nameHash16, const std::shared_ptr<Action>& action) noexcept;

	virtual Anchor getAnchor() const noexcept { return anchor; }
	virtual void setAnchor(const Anchor anchor_) noexcept { anchor = anchor_; }
	virtual void updateSize(const Game& game);

	virtual const sf::Vector2f& DrawPosition() const { return movie.getPosition(); }
	virtual const sf::Vector2f& Position() const { return movie.getPosition(); }
	virtual void Position(const sf::Vector2f& position) { movie.setPosition(position); }
#ifndef USE_SFML_MOVIE_STUB
	virtual sf::Vector2f Size() const noexcept { return size; }
	virtual void Size(const sf::Vector2f& size_)
	{
		size = size_;
		movie.fit(sf::FloatRect(movie.getPosition(), size_));
	}
#else
	virtual sf::Vector2f Size() const { return movie.getSize(); }
	virtual void Size(const sf::Vector2f& size) { movie.setSize(size); }
#endif

	virtual bool Visible() const noexcept { return visible; }
	virtual void Visible(bool visible_) noexcept { visible = visible_; }

	virtual void draw(const Game& game, sf::RenderTarget& target) const;
	virtual void update(Game& game);
	virtual bool getProperty(const std::string_view prop, Variable& var) const;
};
