#pragma once

#include <MY_Game.h>

#include <MY_ResourceManager.h>

#include <MY_Scene_Main.h>
#include <MY_Scene_Menu.h>


MY_Game::MY_Game() :
	Game("game", new MY_Scene_Main(this), true) // initialize our game with a menu scene
{
	sweet::setCursorMode(GLFW_CURSOR_HIDDEN);
}

MY_Game::~MY_Game(){}

void MY_Game::addSplashes(){
	// add default splashes
	//Game::addSplashes();

	// add custom splashes
	//addSplash(new Scene_Splash(this, MY_ResourceManager::globalAssets->getTexture("DEFAULT")->texture, MY_ResourceManager::globalAssets->getAudio("DEFAULT")->sound));
}