#pragma once

#include <MY_Scene_Main.h>
#include <MeshFactory.h>
#include <NumberUtils.h>
#include <RenderOptions.h>

#include <sweet/UI.h>

#include <Building.h>
#include <PointLight.h>
#include <DirectionalLight.h>

#include <RenderSurface.h>
#include <StandardFrameBuffer.h>
#include <RenderOptions.h>
#include <StringUtils.h>
#include <Easing.h>

#include <shader\ShaderComponentTexture.h>
#include <shader\ShaderComponentDiffuse.h>
#include <shader\ShaderComponentDepthOffset.h>
#include <shader\ShaderComponentMVP.h>
#include <ShaderComponentWater.h>

MY_Scene_Main::MY_Scene_Main(Game * _game) :
	MY_Scene_Base(_game),
	currentFloor(0),
	angle(0),
	currentAngle(45),
	currentType("empty"),
	screenSurfaceShader(new Shader("assets/RenderSurface_1", false, true)),
	screenSurface(new RenderSurface(screenSurfaceShader, true)),
	screenFBO(new StandardFrameBuffer(true)),
	money(1000.f),
	morale(100),
	food(100),
	foodGen(0),
	moraleGen(-10),
	moneyGen(0),
	weight(0),
	capacity(10),
	tenants(0),
	waterLevel(0),
	caravanTimer(3),
	floodedFloors(0)
{
	// load stats from file
	Json::Reader reader;
	if(!reader.parse(sweet::FileUtils::readFile("assets/stats.json"), stats, false)){
		Log::error("JSON Parsing failed.");
	}
	
	// memory management
	screenSurface->incrementReferenceCount();
	screenSurfaceShader->incrementReferenceCount();
	screenFBO->incrementReferenceCount();

	// camera
	int gmax = glm::max(GRID_SIZE_X,GRID_SIZE_Z);
	gameCam = new OrthographicCamera(-gmax,gmax, -gmax, gmax, -100,100);
	cameras.push_back(gameCam);
	childTransform->addChild(gameCam);
	activeCamera = gameCam;

	gameCam->yaw = 45;
	gameCam->pitch = -45;
	gameCam->roll = 0;


	// water
	waterShader = new ComponentShaderBase(true);
	//waterShader->addComponent(new ShaderComponentMVP(waterShader));
	waterShader->addComponent(new ShaderComponentWater(waterShader));
	//waterShader->addComponent(new ShaderComponentDiffuse(waterShader));
	waterShader->addComponent(new ShaderComponentDepthOffset(waterShader));
	waterShader->addComponent(new ShaderComponentTexture(waterShader));
	waterShader->compileShader();
	waterShader->nodeName = "water shader";
	waterShader->incrementReferenceCount();

	waterPlane = new MeshEntity(MY_ResourceManager::globalAssets->getMesh("water")->meshes.at(0), waterShader);
	//waterPlane->mesh->pushTexture2D(MY_ResourceManager::globalAssets->getTexture("water")->texture);
	childTransform->addChild(waterPlane);

	// building base
	MeshEntity * foundation = new MeshEntity(MY_ResourceManager::globalAssets->getMesh("foundation")->meshes.at(0), baseShader);
	foundation->mesh->pushTexture2D(MY_ResourceManager::globalAssets->getTexture("foundation")->texture);
	childTransform->addChild(foundation)->scale(GRID_SIZE_X, 1, GRID_SIZE_Z);
	foundation->freezeTransformation();
	foundationOffset = foundation->mesh->calcBoundingBox().height;
	gameCam->firstParent()->translate(0, foundationOffset, 0);

	boat = new MeshEntity(MY_ResourceManager::globalAssets->getMesh("boat")->meshes.at(0), baseShader);
	boat->mesh->pushTexture2D(MY_ResourceManager::globalAssets->getTexture("boat")->texture);
	childTransform->addChild(boat)->translate(16,0,0);
	boat->childTransform->translate(0,0,-boat->mesh->calcBoundingBox().z);

	buildingRoot = new Transform();
	buildingRoot->translate(0, foundationOffset, 0);
	childTransform->addChild(buildingRoot, false);

	// ui stuff
	VerticalLinearLayout * uiPanel = new VerticalLinearLayout(uiLayer->world);
	uiPanel->setPixelHeight(UI_PANEL_HEIGHT);
	uiPanel->setPixelWidth(UI_PANEL_WIDTH);
	uiPanel->setBackgroundColour(1,1,1,1);
	uiPanel->background->mesh->pushTexture2D(MY_ResourceManager::globalAssets->getTexture("uiPanel")->texture);
	uiPanel->verticalAlignment = kTOP;
	//vl->setRenderMode(kTEXTURE);
	uiLayer->addChild(uiPanel);

	HorizontalLinearLayout * buildButtons = new HorizontalLinearLayout(uiLayer->world);
	uiPanel->addChild(buildButtons);
	buildButtons->setRationalWidth(1.f, uiPanel);
	buildButtons->marginLeft.setPixelSize(2);
	buildButtons->marginTop.setPixelSize(16);

	TextArea * buildDescription = new TextArea(uiLayer->world, font, textShader);
	uiPanel->addChild(buildDescription);
	buildDescription->setWidth(UI_PANEL_WIDTH);
	buildDescription->boxSizing = kBORDER_BOX;
	buildDescription->setHeight(77*2);
	buildDescription->setMargin(4,0);
	buildDescription->setMarginBottom(28);
	buildDescription->setWrapMode(kWORD);
	buildDescription->setRenderMode(kTEXTURE);
	buildDescription->verticalAlignment = kTOP;
	buildDescription->setText("Lorem ipsem dolor sit amet. Blah blhhadi wajd woadjw iadjowa jdwad jwaodjwaiodj awidoj awodj waij");
	buildDescription->setMouseEnabled(true);

	Texture * btnTexNormal = MY_ResourceManager::globalAssets->getTexture("btn-normal")->texture;
	Texture * btnTexOver = MY_ResourceManager::globalAssets->getTexture("btn-over")->texture;
	Texture * btnTexDown = MY_ResourceManager::globalAssets->getTexture("btn-down")->texture;
	auto btnOnIn = [btnTexOver](sweet::Event * _event){
		((NodeUI *)(_event->getIntData("target")))->background->mesh->replaceTextures(btnTexOver);
	};auto btnOnDown = [btnTexDown](sweet::Event * _event){
		((NodeUI *)(_event->getIntData("target")))->background->mesh->replaceTextures(btnTexDown);
	};auto btnOnOutOrUp = [btnTexNormal](sweet::Event * _event){
		((NodeUI *)(_event->getIntData("target")))->background->mesh->replaceTextures(btnTexNormal);
	};auto btnOnClick = [this](sweet::Event * _event){
		setType(((NodeUI *)(_event->getIntData("target")))->nodeName);
	};

	std::vector<std::string> btns;
	btns.push_back("support");
	btns.push_back("balloon");
	btns.push_back("room");
	btns.push_back("stairs");
	btns.push_back("foodcourt");
	btns.push_back("supplies");
	btns.push_back("empty");
	for(auto s : btns){
		NodeUI * t = new NodeUI(uiLayer->world);
		buildButtons->addChild(t);
		t->setMouseEnabled(true);
		t->setWidth(32);
		t->setHeight(32);
		t->setMargin(2);
		t->nodeName = s;
		NodeUI * icon = new NodeUI(uiLayer->world);
		t->addChild(icon);
		icon->setWidth(1.f);
		icon->setHeight(1.f);
		icon->background->mesh->pushTexture2D(MY_ResourceManager::globalAssets->getTexture("btn-"+s)->texture);
		
		t->background->mesh->pushTexture2D(btnTexNormal);
		t->eventManager->addEventListener("mousein", btnOnIn);
		t->eventManager->addEventListener("mousedown", btnOnDown);
		t->eventManager->addEventListener("mouseup", btnOnOutOrUp);
		t->eventManager->addEventListener("mouseout", btnOnOutOrUp);
		t->eventManager->addEventListener("click", btnOnClick);
	}
	

	/*NodeUI * test = new NodeUI(uiLayer->world);
	uiPanel->addChild(test);
	test->boxSizing = kCONTENT_BOX;
	test->setMarginTop(28);
	test->setMarginLeft(160);
	test->setWidth(5);
	test->setHeight(5);*/

	float hoffset = 160.f;
	{
	HorizontalLinearLayout * hl = new HorizontalLinearLayout(uiLayer->world);
	uiPanel->addChild(hl);
	hl->setMarginLeft(hoffset);
	hl->setHeight(10);
	hl->boxSizing = kCONTENT_BOX;
	arrowMoney = new NodeUI(uiLayer->world);
	hl->addChild(arrowMoney);
	arrowMoney->setHeight(10);
	arrowMoney->setWidth(10);
	arrowMoney->setMarginLeft(2);
	arrowMoney->background->mesh->pushTexture2D(MY_ResourceManager::globalAssets->getTexture("arrow-up")->texture);
	TextLabelControlled * lbl = new TextLabelControlled(&money, -FLT_MAX, FLT_MAX, uiLayer->world, font, textShader);
	hl->addChild(lbl);
	lbl->setHeight(10);
	lbl->decimals = 0;
	lbl->setRenderMode(kTEXTURE);
	}{
	HorizontalLinearLayout * hl = new HorizontalLinearLayout(uiLayer->world);
	uiPanel->addChild(hl);
	hl->boxSizing = kCONTENT_BOX;
	hl->setMarginTop(2);
	hl->setMarginLeft(hoffset);
	hl->setHeight(10);
	TextLabelControlled * lbl = new TextLabelControlled(&tenants, 0, FLT_MAX, uiLayer->world, font, textShader);
	hl->addChild(lbl);
	lbl->setRenderMode(kTEXTURE);
	lbl->decimals = 0;
	lbl->setHeight(10);
	lbl->setWidth(32);
	TextLabelControlled * lbl2 = new TextLabelControlled(&capacity, 0, FLT_MAX, uiLayer->world, font, textShader);
	hl->addChild(lbl2);
	lbl2->setRenderMode(kTEXTURE);
	lbl2->decimals = 0;
	lbl2->prefix = L"/";
	lbl2->setHeight(10);
	lbl2->setWidth(50);
	}{
	HorizontalLinearLayout * hl = new HorizontalLinearLayout(uiLayer->world);
	uiPanel->addChild(hl);
	hl->setMarginLeft(hoffset);
	hl->setHeight(10);
	hl->setMarginTop(2);
	hl->boxSizing = kCONTENT_BOX;
	arrowMorale = new NodeUI(uiLayer->world);
	hl->addChild(arrowMorale);
	arrowMorale->setHeight(10);
	arrowMorale->setWidth(10);
	arrowMorale->setMarginLeft(2);
	arrowMorale->background->mesh->pushTexture2D(MY_ResourceManager::globalAssets->getTexture("arrow-up")->texture);
	TextLabelControlled * lbl = new TextLabelControlled(&morale, 0, 100, uiLayer->world, font, textShader);
	hl->addChild(lbl);
	lbl->setHeight(10);
	lbl->decimals = 0;
	lbl->suffix = L"/100";
	lbl->setRenderMode(kTEXTURE);
	}{
	HorizontalLinearLayout * hl = new HorizontalLinearLayout(uiLayer->world);
	uiPanel->addChild(hl);
	hl->setMarginLeft(hoffset);
	hl->setHeight(10);
	hl->setMarginTop(2);
	hl->boxSizing = kCONTENT_BOX;
	arrowFood = new NodeUI(uiLayer->world);
	hl->addChild(arrowFood);
	arrowFood->setHeight(10);
	arrowFood->setWidth(10);
	arrowFood->setMarginLeft(2);
	arrowFood->background->mesh->pushTexture2D(MY_ResourceManager::globalAssets->getTexture("arrow-up")->texture);
	TextLabelControlled * lbl = new TextLabelControlled(&food, 0, FLT_MAX, uiLayer->world, font, textShader);
	hl->addChild(lbl);
	lbl->setHeight(10);
	lbl->decimals = 0;
	lbl->setRenderMode(kTEXTURE);
	}{
	TextLabelControlled * lbl = new TextLabelControlled(&weight, 0, FLT_MAX, uiLayer->world, font, textShader);
	uiPanel->addChild(lbl);
	lbl->boxSizing = kCONTENT_BOX;
	lbl->prefix = L"-";
	lbl->setMarginTop(2);
	lbl->setMarginLeft(hoffset);
	lbl->setHeight(10);
	lbl->setRenderMode(kTEXTURE);
	lbl->decimals = 0;
	}
	
	lblMsg = new TextArea(uiLayer->world, font, textShader);
	uiPanel->addChild(lblMsg);
	lblMsg->setWidth(UI_PANEL_WIDTH);
	lblMsg->boxSizing = kBORDER_BOX;
	lblMsg->setHeight(77*2);
	lblMsg->setMargin(4,0);
	lblMsg->setMarginTop(28);
	lblMsg->setWrapMode(kWORD);
	lblMsg->setRenderMode(kTEXTURE);
	lblMsg->verticalAlignment = kTOP;
	lblMsg->setText("Lorem ipsem dolor sit amet. Blah blhhadi wajd woadjw iadjowa jdwad jwaodjwaiodj awidoj awodj waij");

	gameplayTick = new Timeout(getStat("tickDuration"), [this](sweet::Event * _event){
		caravanTimer -= 1;
		if(caravanTimer == 0){
			caravanTimer = getStat("caravans.delay");
			int numTenants = sweet::NumberUtils::randomInt(1,getStat("caravans.delay"));
			for(unsigned long int i = 0; i < numTenants && (capacity - tenants > 0.99f); ++i){
				addTenant();
			}
		}

		money += moneyGen * getStat("tickMultipliers.money");
		morale += moraleGen * getStat("tickMultipliers.morale");
		food += foodGen * getStat("tickMultipliers.food");

		// if out of food, remainder is taken as a hit to morale and there's a chance to lose tenants
		if(food < 0){
			morale += food;
			food = 0;
			for(unsigned long int i = 0; i < glm::ceil(tenants*0.75f); ++i){
				if(sweet::NumberUtils::randomFloat() > morale/100.f){
					removeTenant();
				}
			}
		}

		float oldWaterLevel = glm::floor(waterLevel);
		waterLevel += glm::max(0.f,weight) * getStat("tickMultipliers.weight");
		while(waterLevel > oldWaterLevel + 0.99f){
			oldWaterLevel += 1;
			floodFloor();
		}

		updateStats();

		gameplayTick->restart();
	});
	gameplayTick->eventManager->addEventListener("progress", [this](sweet::Event * _event){
		float p = _event->getFloatData("progress");
		float wait = 0.2f;
		float away = 0.2f;
		if(p < wait/2.f){
			// second half of turn while docked
			p = Easing::easeOutCubic(p, 0.5, -0.5, wait/2.f);
			boat->childTransform->setOrientation(glm::angleAxis(180*p, glm::vec3(0,1,0)));
			p = 1.f;
		}else if(p > 1.f-wait/2.f){
			// first half of turn while docked
			p = Easing::easeInCubic(p-(1.f-wait/2.f), 1, -0.5, wait/2.f);
			boat->childTransform->setOrientation(glm::angleAxis(180*p, glm::vec3(0,1,0)));
			p = 1.f;
		}else if(p < 0.5-away/2.f){
			// travelling away
			p = Easing::easeInOutCubic(p-wait/2.f, 1, -1, 0.5f-wait/2.f-away/2.f);
			boat->childTransform->setOrientation(glm::angleAxis(0.f, glm::vec3(0,1,0)));
		}else if(p > 0.5+away/2.f){
			// travelling towards
			p = Easing::easeInOutCubic(p-0.5-away/2.f, 0, 1, 0.5f-wait/2.f-away/2.f);
			boat->childTransform->setOrientation(glm::angleAxis(180.f, glm::vec3(0,1,0)));
		}else{
			// waiting while away
			// also turn around
			p = Easing::easeInOutCubic(p-0.5f+away/2.f, 0, 1, away);
			boat->childTransform->setOrientation(glm::angleAxis(180*p, glm::vec3(0,1,0)));
			p = 0;
		}
		
		boat->firstParent()->translate(0.5, waterPlane->firstParent()->getTranslationVector().y, GRID_SIZE_Z*3 + (GRID_SIZE_Z*0.5f - GRID_SIZE_Z*3)*p, false);
	});
	childTransform->addChild(gameplayTick, false);
	gameplayTick->start();

	alertTimeout = new Timeout(2.5f, [this](sweet::Event * _event){
		lblMsg->setText("");
	});
	childTransform->addChild(alertTimeout, false);





	// setup starting stats
	bgColour = glm::vec3(getStat("bg.r"),getStat("bg.g"),getStat("bg.b"));

	money = getStat("startingResources.money");
	morale = getStat("startingResources.morale");
	food = getStat("startingResources.food");
	weight = getStat("startingResources.weight");
	capacity = getStat("startingResources.capacity");
	caravanTimer = getStat("caravans.delay");
	for(unsigned long int i = 0; i < getStat("startingResources.tenants"); ++i){
		addTenant();
	}
	int numFloors = getStat("startingResources.floors");
	placeFloor();
	for(unsigned long int i = 1; i < numFloors; ++i){
		placeBuilding("stairs", glm::ivec3(1,i-1,1), true);
	}

	
	// add the selector thing (this needs to be done after floors are made)
	selectorThing = new MeshEntity(MY_ResourceManager::globalAssets->getMesh("cube")->meshes.at(0), baseShader);
	for(auto & v : selectorThing->mesh->vertices){
		v.alpha = 0.25f;
	}
	floors.at(currentFloor)->wallContainerOpaque->addChild(selectorThing);
	

	update(&sweet::step);
	pause();


	NodeUI * tutorial = new NodeUI(uiLayer->world);
	uiLayer->addChild(tutorial);
	tutorial->setRationalWidth(1.f, uiLayer);
	tutorial->setRationalHeight(1.f, uiLayer);
	tutorial->background->mesh->pushTexture2D(MY_ResourceManager::globalAssets->getTexture("tutorial")->texture);
	tutorial->setMouseEnabled(true);
	tutorial->eventManager->addEventListener("click", [this, tutorial](sweet::Event * _event){
		tutorial->setVisible(false);
		resume();
	});
}

MY_Scene_Main::~MY_Scene_Main(){
	// we need to destruct the scene elements before the physics world to avoid memory issues
	deleteChildTransform();
	
	waterShader->decrementAndDelete();

	// memory management
	screenSurface->decrementAndDelete();
	screenSurfaceShader->decrementAndDelete();
	screenFBO->decrementAndDelete();
}

void MY_Scene_Main::update(Step * _step){
	// Screen shader update
	// Screen shaders are typically loaded from a file instead of built using components, so to update their uniforms
	// we need to use the OpenGL API calls
	screenSurfaceShader->bindShader(); // remember that we have to bind the shader before it can be updated
	GLint test = glGetUniformLocation(screenSurfaceShader->getProgramId(), "time");
	checkForGlError(0);
	if(test != -1){
		glUniform1f(test, _step->time);
		checkForGlError(0);
	}

	
	
	if(keyboard->keyJustDown(GLFW_KEY_L)){
		screenSurfaceShader->unload();
		screenSurfaceShader->loadFromFile(screenSurfaceShader->vertSource, screenSurfaceShader->fragSource);
		screenSurfaceShader->load();
	}

	if(keyboard->keyJustDown(GLFW_KEY_P)){
		if(gameplayTick->active){
			pause();
		}else{
			resume();
		}
	}

	// resize camera to fit width-wise and maintain aspect ratio height-wise
	glm::uvec2 sd = sweet::getWindowDimensions();
	float ar = (float)sd.y / (sd.x - UI_PANEL_WIDTH);
	gameCam->resize(gameCam->left, gameCam->right, gameCam->getWidth()*ar * -0.5f, gameCam->getWidth()*ar * 0.5f);
	
	glm::vec3 camPos = gameCam->firstParent()->getTranslationVector();
	glm::ivec3 cursorPos = getIsometricCursorPos();
	
	selectorThing->firstParent()->translate(glm::vec3(cursorPos.x - GRID_SIZE_X/2.f, 0, cursorPos.z - GRID_SIZE_Z/2.f), false);
	

	for(unsigned long int i = 0; i < floors.size(); ++i){
		floors.at(i)->updateVisibility(currentFloor + floodedFloors, angle);
	}
	camPos.y += ((currentFloor + floodedFloors + foundationOffset) - gameCam->firstParent()->getTranslationVector().y)*0.1f;

	float angleDif = ((angle+0.5f)*90.f - currentAngle);

	currentAngle += angleDif * 0.1f;
	gameCam->yaw = currentAngle;
	gameCam->firstParent()->translate(camPos, false);

	// player input
	
	// rotate view
	if(keyboard->keyJustDown(GLFW_KEY_LEFT) || keyboard->keyJustDown(GLFW_KEY_A)){
		angle -= 1;
		if(angle < 0){
			angle = 3;
			currentAngle += 360.f;
		}
	}if(keyboard->keyJustDown(GLFW_KEY_RIGHT) || keyboard->keyJustDown(GLFW_KEY_D)){
		angle += 1;
		if(angle > 3){
			angle = 0;
			currentAngle -= 360.f;
		}
	}

	// scroll floors
	int floorChange = 0;
	if(keyboard->keyJustDown(GLFW_KEY_UP) || keyboard->keyJustDown(GLFW_KEY_W) || mouse->getMouseWheelDelta() > 0.5f){
		if(currentFloor < floors.size()-1){
			floorChange += 1;
		}
	}if(keyboard->keyJustDown(GLFW_KEY_DOWN) || keyboard->keyJustDown(GLFW_KEY_S) || mouse->getMouseWheelDelta() < -0.5f){
		if(currentFloor > 0){
			floorChange -= 1;
		}
	}
	if(floorChange != 0){
		setFloor(currentFloor + glm::sign(floorChange));
	}



	
		
	// non-paused interaction
	if(gameplayTick->active){
		// move water visual upwards to match actual water level
		waterPlane->firstParent()->translate(0,glm::min(0.005f, glm::max(0.f, waterLevel + foundationOffset - 0.4f) - waterPlane->firstParent()->getTranslationVector().y),0);

		if(mouse->leftJustPressed()){
			const AssetBuilding * ab = MY_ResourceManager::getBuilding(currentType);
			// make sure the cursor is within bounds
			cursorPos += glm::ivec3(1,0,1);
			bool validSpot;
			if(ab->empty){
				validSpot = 
					cursorPos.x >= 0 && cursorPos.x < GRID_SIZE_X+2 &&
					cursorPos.z >= 0 && cursorPos.z < GRID_SIZE_Z+2;
			}else if(ab->aerial){
				// along edge, but not on a corner
				bool xedge = cursorPos.x == 0 || cursorPos.x == GRID_SIZE_X+1;
				bool zedge = cursorPos.z == 0 || cursorPos.z == GRID_SIZE_Z+1; 
				validSpot =
					(xedge ^ zedge) &&
					cursorPos.x >= 0 && cursorPos.x < GRID_SIZE_X+2 &&
					cursorPos.z >= 0 && cursorPos.z < GRID_SIZE_Z+2;
			}else{
				validSpot =
					cursorPos.x >= 1 && cursorPos.x < GRID_SIZE_X+1 &&
					cursorPos.z >= 1 && cursorPos.z < GRID_SIZE_Z+1;
			}
			if(validSpot){
			
				Cell * cell = getCell(cursorPos);

				std::stringstream ss;
				ss << "Clicked floor " << currentFloor << ", cell " << cursorPos.x << " " << cursorPos.z << std::endl;
				ss << "cell type is " << cell->building->definition->id << ", user type is " << currentType;
				Log::info(ss.str());

				if(cell->building->definition->id != currentType && cell->building->definition->id != "blocked"){
					if(cell->building->definition->empty || ab->empty){
						placeBuilding(currentType, cursorPos, false);
					}else{
						alert("You can't place a building here.");
					}
				}else{
					alert("You can't place a building here.");
				}
			}else{
				alert("You can't place that building here.");
			}
		}
	}



	// Scene update
	MY_Scene_Base::update(_step);
}

void MY_Scene_Main::pause(){
	gameplayTick->pause();

	selectorThing->setVisible(false);
}
void MY_Scene_Main::resume(){
	gameplayTick->start();
	selectorThing->setVisible(true);
}

void MY_Scene_Main::updateStats(){
	if(moraleGen < -0.5){
		arrowMorale->background->mesh->replaceTextures(MY_ResourceManager::globalAssets->getTexture("arrow-down")->texture);
	}else if(moraleGen > 0.5){
		arrowMorale->background->mesh->replaceTextures(MY_ResourceManager::globalAssets->getTexture("arrow-up")->texture);
	}else{
		arrowMorale->background->mesh->replaceTextures(MY_ResourceManager::globalAssets->getTexture("transparent")->texture);
	}
	if(moneyGen < -0.5){
		arrowMoney->background->mesh->replaceTextures(MY_ResourceManager::globalAssets->getTexture("arrow-down")->texture);
	}else if(moneyGen > 0.5){
		arrowMoney->background->mesh->replaceTextures(MY_ResourceManager::globalAssets->getTexture("arrow-up")->texture);
	}else{
		arrowMoney->background->mesh->replaceTextures(MY_ResourceManager::globalAssets->getTexture("transparent")->texture);
	}
	if(foodGen < -0.5){
		arrowFood->background->mesh->replaceTextures(MY_ResourceManager::globalAssets->getTexture("arrow-down")->texture);
	}else if(foodGen > 0.5){
		arrowFood->background->mesh->replaceTextures(MY_ResourceManager::globalAssets->getTexture("arrow-up")->texture);
	}else{
		arrowFood->background->mesh->replaceTextures(MY_ResourceManager::globalAssets->getTexture("transparent")->texture);
	}
}

void MY_Scene_Main::setFloor(unsigned long int _floor){
	currentFloor = _floor;
	
	// update selector to the current floor
	selectorThing->firstParent()->firstParent()->removeChild(selectorThing->firstParent());
	floors.at(currentFloor)->wallContainerOpaque->addChild(selectorThing->firstParent());
		
	// trigger animations
	if(currentFloor != 0){
		floors.at(currentFloor-1)->boing->restart();
	}
	floors.at(currentFloor)->boing->restart();
}


void MY_Scene_Main::render(sweet::MatrixStack * _matrixStack, RenderOptions * _renderOptions){
	// keep our screen framebuffer up-to-date with the current viewport
	screenFBO->resize(_renderOptions->viewPortDimensions.width, _renderOptions->viewPortDimensions.height);

	// bind our screen framebuffer
	FrameBufferInterface::pushFbo(screenFBO);

	// render the scene
	_renderOptions->setViewPort(UI_PANEL_WIDTH,0,screenFBO->width - UI_PANEL_WIDTH, screenFBO->height);
	float f = (1.f-(gameCam->firstParent()->getTranslationVector().y - waterPlane->firstParent()->getTranslationVector().y)/50.f) * (glm::sin(sweet::lastTimestamp*(1.f/gameplayTick->targetSeconds*3.f))*0.5f+1);
	_renderOptions->setClearColour(bgColour.r*f, bgColour.g*f, bgColour.b*f,1);
	MY_Scene_Base::render(_matrixStack, _renderOptions);

	// unbind our screen framebuffer, rebinding the previously bound framebuffer
	// since we didn't have one bound before, this will be the default framebuffer (i.e. the one visible to the player)
	FrameBufferInterface::popFbo();
	
	_renderOptions->setViewPort(0,0,screenFBO->width, screenFBO->height);
	// render our screen framebuffer using the standard render surface
	screenSurface->render(screenFBO->getTextureId());

	// render the uiLayer after the screen surface in order to avoid hiding it through shader code
	uiLayer->render(_matrixStack, _renderOptions);
}

void MY_Scene_Main::enableDebug(){
	MY_Scene_Base::enableDebug();
}
void MY_Scene_Main::disableDebug(){
	MY_Scene_Base::disableDebug();
}


glm::ivec3 MY_Scene_Main::getIsometricCursorPos(){
	// calculate in-game isometric cursor position
	glm::uvec2 sd = sweet::getWindowDimensions();
	glm::vec3 camPos = gameCam->firstParent()->getTranslationVector();
	glm::vec3 start = gameCam->screenToWorld(glm::vec3((mouse->mouseX()-UI_PANEL_WIDTH)/(sd.x - UI_PANEL_WIDTH), mouse->mouseY()/sd.y, gameCam->nearClip), sd);
	glm::vec3 dir = gameCam->forwardVectorRotated;
	glm::vec3 norm(0,1,0);
	glm::vec3 cursorPos(0);

	float d = glm::dot(norm, dir);
	if(glm::abs(d) > FLT_EPSILON){
		float t = glm::dot(glm::vec3(0,currentFloor + floodedFloors + foundationOffset,0) - start, norm) / d;
		if(glm::abs(t) > FLT_EPSILON){
			cursorPos = start + dir * t;
		}
	}
	return glm::ivec3( glm::floor(cursorPos.x) + GRID_SIZE_X/2.f, currentFloor, glm::floor(cursorPos.z) + GRID_SIZE_Z/2.f );
}

void MY_Scene_Main::setType(std::string _buildingType){
	// if the type hasn't changed, return early without doing anything
	if(currentType == _buildingType){
		return;
	}

	currentType = _buildingType;

	selectorThing->mesh->vertices.clear();
	selectorThing->mesh->indices.clear();
	selectorThing->mesh->insertVertices(*MY_ResourceManager::getBuilding(currentType)->meshes.at(0));
	for(auto & v : selectorThing->mesh->vertices){
		v.alpha = 0.25f;
	}
	selectorThing->mesh->replaceTextures(MY_ResourceManager::getBuilding(currentType)->meshes.at(0)->textures.at(0));
}

void MY_Scene_Main::placeBuilding(std::string _buildingType, glm::ivec3 _position, bool _free){
	
	// if the player can't afford it, let them know and return early
	if(!_free && money - MY_ResourceManager::getBuilding(_buildingType)->cost < 0){
		alert("You can't afford this building.");
		return;
	}
	
	removeBuilding(_position);

	const AssetBuilding * ab = MY_ResourceManager::getBuilding(_buildingType);
	Building * b = new Building(ab, baseShader);
	Floor * floor = floors.at(_position.y);
	Transform * p = (ab->aerial ? floor->wallContainerOpaque : floor->cellContainer);
	p->addChild(b)->translate(_position.x - GRID_SIZE_X/2.f - 1, 0, _position.z - GRID_SIZE_Z/2.f - 1, false);
	floor->cells[_position.x][_position.z]->building = b;
	
	if(ab->aerial){
		float angle = 0;
		if(_position.x == 0){
			angle = 0;
		}else if(_position.x == GRID_SIZE_X+1){
			angle = 180;
			b->firstParent()->translate(1,0,1);
		}else if(_position.z == 0){
			angle = 270;
			b->firstParent()->translate(1,0,0);
		}else if(_position.z  == GRID_SIZE_Z+1){
			angle = 90;
			b->firstParent()->translate(0,0,1);
		}
		b->firstParent()->rotate(angle, 0,1,0, kOBJECT);
	}

	weight += ab->weight;
	foodGen += ab->generates.food;
	moraleGen += ab->generates.morale;
	moneyGen += ab->generates.money;
	capacity += ab->capacity;

	if(b->definition->support && _position.y < floors.size()-1){
		placeBuilding("empty", _position + glm::ivec3(0,1,0), true);
	}

	if(!_free){
		money -= b->definition->cost;
	}

	// if we're placing stairs on the top floor, add a new floor
	if(_position.y == floors.size()-1 && _buildingType == "stairs"){
		placeFloor();
	}

	updateStats();
}

void MY_Scene_Main::removeBuilding(glm::ivec3 _position){
	Cell * cell = getCell(_position);
	if(cell->building != nullptr){
		Floor * floor = floors.at(_position.y);
		Transform * p = (cell->building->definition->aerial ? floor->wallContainerOpaque : floor->cellContainer);
		p->removeChild(cell->building->firstParent());
		const AssetBuilding * ab = cell->building->definition;
		
		weight -= ab->weight;
		foodGen -= ab->generates.food;
		moraleGen -= ab->generates.morale;
		moneyGen -= ab->generates.money;
		capacity -= ab->capacity;

		delete cell->building->firstParent();
		cell->building = nullptr;
	}
	updateStats();
}

void MY_Scene_Main::placeFloor(){
	unsigned long int y = floors.size() + floodedFloors;
	Floor * floor = new Floor(y, baseShader);
	buildingRoot->addChild(floor, false);
	floor->translate(0, y, 0);
	floors.push_back(floor);
	for(unsigned long int x = 0; x < GRID_SIZE_X+2; ++x){
		for(unsigned long int z = 0; z < GRID_SIZE_Z+2; ++z){
			bool xedge = x == 0 || x == GRID_SIZE_X+1;
			bool zedge = z == 0 || z == GRID_SIZE_Z+1;
			if(floors.size() == 1 || floors.at(floors.size()-2)->cells[x][z]->building->definition->support
				||
				(xedge || zedge)
				){
				placeBuilding("empty", glm::ivec3(x,y-floodedFloors,z), true);
			}else{
				placeBuilding("blocked", glm::ivec3(x,y-floodedFloors,z), true);
			}
		}
	}

	floor->updateVisibility(currentFloor + floodedFloors, angle);
}

Cell * MY_Scene_Main::getCell(glm::ivec3 _position){
	return floors.at(_position.y)->cells[_position.x][_position.z];
}


void MY_Scene_Main::addTenant(){
	tenants += 1;
	
	changeStat("tenant", true);
	
}
void MY_Scene_Main::removeTenant(){
	tenants -= 1;
	
	changeStat("tenant", false);

	if(tenants <= FLT_EPSILON){
		tenants = 0;
		// TODO: game over
	}
}

void MY_Scene_Main::changeStat(std::string _statChange, bool _positive){
	float mult = _positive ? 1 : -1;
	
	std::string s = "statChanges." + _statChange + ".";

	morale +=     mult * getStat(s + "morale");
	food +=       mult * getStat(s + "food");
	money +=      mult * getStat(s + "money");
	moraleGen +=  mult * getStat(s + "moraleGen");
	foodGen +=    mult * getStat(s + "foodGen");
	moneyGen +=   mult * getStat(s + "moneyGen");
	moneyGen +=   mult * getStat(s + "moneyGen");
	weight +=     mult * getStat(s + "weight");

}

void MY_Scene_Main::alert(std::string _msg){
	lblMsg->setText(_msg);
	alertTimeout->restart();
}


void MY_Scene_Main::floodFloor(){
	floodedFloors += 1;

	// lose functionality of all buildings on floor
	// and a fraction of the weight
	Floor * floor = floors.front();
	for(unsigned long int x = 0; x < GRID_SIZE_X; ++x){
	for(unsigned long int z = 0; z < GRID_SIZE_X; ++z){
		const AssetBuilding * ab = floor->cells[x][z]->building->definition;
		
		weight -= ab->weight*getStat("floodedWeight");
		foodGen -= ab->generates.food;
		moraleGen -= ab->generates.morale;
		moneyGen -= ab->generates.money;
		capacity -= ab->capacity;
	}
	}

	// if tenants are now higher than capacity, take a morale hit
	morale -= glm::min(0.f, tenants - capacity);

	// based on morale, possibly lose some tenants
	for(unsigned long int i = 0; i < glm::ceil(tenants*0.75f); ++i){
		if(sweet::NumberUtils::randomFloat() > morale/100.f){
			removeTenant();
		}
	}

	floor->updateVisibility(floor->height+1,angle);
	floors.erase(floors.begin());


	// if this was your last floor, trigger gameover
	if(floors.size() == 0){
		// TODO: gameover
		return;
	}

	if(currentFloor == 0){
		setFloor(0);
	}else{
		setFloor(currentFloor-1);
	}

}

float MY_Scene_Main::getStat(std::string _statName){
	std::vector<std::string> sv = sweet::StringUtils::split(_statName, '.');
	Json::Value v = stats;
	for(unsigned long int i = 0; i < sv.size(); ++i){
		if(!v.isMember(sv.at(i))){
			Log::error("stat doesn't exist");
		}
		v = v[sv.at(i)];
	}
	return v.asFloat();
}