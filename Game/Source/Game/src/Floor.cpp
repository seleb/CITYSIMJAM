#pragma once

#include <Floor.h>
#include <MY_ResourceManager.h>

Floor::Floor(unsigned long int _height, Shader * _shader) : 
	height(_height)
{
	wallContainer = new Transform();
	cellContainer = new Transform();
	addChild(cellContainer,false);
	addChild(wallContainer,false);
	MeshInterface * wallMesh = MY_ResourceManager::globalAssets->getMesh("wall")->meshes.at(0);
	for(unsigned long int i = 0; i < 4; ++i){
		MeshEntity * wall = new MeshEntity(wallMesh, _shader);
		wallContainer->addChild(wall)->translate(2,0,2)->rotate(i*90.f,0,1,0,kOBJECT);
		walls.push_back(wall);
	}
}

Floor::~Floor(){

}

void Floor::updateVisibility(unsigned long int _height, unsigned long int _angle){
	if(height == _height){
		setVisible(true);
		cellContainer->setVisible(true);
		wallContainer->setVisible(true);
		for(unsigned long int i = 0; i < walls.size(); ++i){
			walls.at(i)->setVisible(i == _angle || i == _angle+1 || i == _angle-3);
		}
	}else{
		cellContainer->setVisible(false);
		if(height < _height){
			setVisible(true);
			wallContainer->setVisible(true);
			for(auto wall : walls){
				wall->setVisible(true);
			}
		}else{
			setVisible(false);
			wallContainer->setVisible(false);
		}
	}
}