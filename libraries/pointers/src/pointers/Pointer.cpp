//
//  Created by Sam Gondelman 10/19/2017
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "Pointer.h"

#include <DependencyManager.h>
#include "PickManager.h"
#include "PointerManager.h"

Pointer::~Pointer() {
    DependencyManager::get<PickManager>()->removePick(_pickUID);
}

void Pointer::enable() {
    DependencyManager::get<PickManager>()->enablePick(_pickUID);
    withWriteLock([&] {
        _enabled = true;
    });
}

void Pointer::disable() {
    // Disable the pointer first, then the pick, so someone can't try to use it while it's in a bad state
    withWriteLock([&] {
        _enabled = false;
    });
    DependencyManager::get<PickManager>()->disablePick(_pickUID);
}

PickResultPointer Pointer::getPrevPickResult() {
    return DependencyManager::get<PickManager>()->getPrevPickResult(_pickUID);
}

void Pointer::setPrecisionPicking(bool precisionPicking) {
    DependencyManager::get<PickManager>()->setPrecisionPicking(_pickUID, precisionPicking);
}

void Pointer::setIgnoreItems(const QVector<QUuid>& ignoreItems) const {
    DependencyManager::get<PickManager>()->setIgnoreItems(_pickUID, ignoreItems);
}

void Pointer::setIncludeItems(const QVector<QUuid>& includeItems) const {
    DependencyManager::get<PickManager>()->setIncludeItems(_pickUID, includeItems);
}

bool Pointer::isLeftHand() const {
    return DependencyManager::get<PickManager>()->isLeftHand(_pickUID);
}

bool Pointer::isRightHand() const {
    return DependencyManager::get<PickManager>()->isRightHand(_pickUID);
}

bool Pointer::isMouse() const {
    return DependencyManager::get<PickManager>()->isMouse(_pickUID);
}

void Pointer::update(unsigned int pointerID, float deltaTime) {
    // This only needs to be a read lock because update won't change any of the properties that can be modified from scripts
    withReadLock([&] {
        auto pickResult = getPrevPickResult();
        updateVisuals(pickResult);
        generatePointerEvents(pointerID, pickResult);
    });
}

void Pointer::generatePointerEvents(unsigned int pointerID, const PickResultPointer& pickResult) {
    // TODO: avatars/HUD?
    auto pointerManager = DependencyManager::get<PointerManager>();

    // NOTE: After this loop: _prevButtons = buttons that were removed
    // If switching to disabled or should stop triggering, release all buttons
    Buttons buttons;
    Buttons newButtons;
    Buttons sameButtons;
    const std::string PRIMARY_BUTTON = "Primary";
    bool primaryPressed = false;
    if (_enabled && shouldTrigger(pickResult)) {
        buttons = getPressedButtons();
        primaryPressed = buttons.find(PRIMARY_BUTTON) != buttons.end();
        for (const std::string& button : buttons) {
            if (_prevButtons.find(button) == _prevButtons.end()) {
                newButtons.insert(button);
            } else {
                sameButtons.insert(button);
                _prevButtons.erase(button);
            }
        }
    }

    // Hover events
    bool doHover = shouldHover(pickResult);
    Pointer::PickedObject hoveredObject = getHoveredObject(pickResult);
    PointerEvent hoveredEvent = buildPointerEvent(hoveredObject, pickResult);
    hoveredEvent.setType(PointerEvent::Move);
    hoveredEvent.setID(pointerID);
    bool releaseOnHoverLeave = !primaryPressed || (!_enabled && _prevEnabled) || (!doHover && _prevDoHover);
    hoveredEvent.setReleaseOnHoverLeave(releaseOnHoverLeave);

    // if shouldHover && !_prevDoHover, only send hoverBegin
    if (_enabled && _hover && doHover && !_prevDoHover) {
        if (hoveredObject.type == ENTITY) {
            emit pointerManager->hoverBeginEntity(hoveredObject.objectID, hoveredEvent);
        } else if (hoveredObject.type == OVERLAY) {
            emit pointerManager->hoverBeginOverlay(hoveredObject.objectID, hoveredEvent);
        } else if (hoveredObject.type == HUD) {
            emit pointerManager->hoverBeginHUD(hoveredEvent);
        }
    } else if (_enabled && _hover && doHover) {
        if (hoveredObject.type == OVERLAY) {
            if (_prevHoveredObject.type == OVERLAY) {
                if (hoveredObject.objectID == _prevHoveredObject.objectID) {
                    emit pointerManager->hoverContinueOverlay(hoveredObject.objectID, hoveredEvent);
                } else {
                    PointerEvent prevHoveredEvent = buildPointerEvent(_prevHoveredObject, pickResult);
                    prevHoveredEvent.setID(pointerID);
                    prevHoveredEvent.setReleaseOnHoverLeave(releaseOnHoverLeave);
                    emit pointerManager->hoverEndOverlay(_prevHoveredObject.objectID, prevHoveredEvent);
                    emit pointerManager->hoverBeginOverlay(hoveredObject.objectID, hoveredEvent);
                }
            } else {
                emit pointerManager->hoverBeginOverlay(hoveredObject.objectID, hoveredEvent);
                if (_prevHoveredObject.type == ENTITY) {
                    emit pointerManager->hoverEndEntity(_prevHoveredObject.objectID, hoveredEvent);
                } else if (_prevHoveredObject.type == HUD) {
                    emit pointerManager->hoverEndHUD(hoveredEvent);
                }
            }
        }

        // TODO: this is basically repeated code.  is there a way to clean it up?
        if (hoveredObject.type == ENTITY) {
            if (_prevHoveredObject.type == ENTITY) {
                if (hoveredObject.objectID == _prevHoveredObject.objectID) {
                    emit pointerManager->hoverContinueEntity(hoveredObject.objectID, hoveredEvent);
                } else {
                    PointerEvent prevHoveredEvent = buildPointerEvent(_prevHoveredObject, pickResult);
                    prevHoveredEvent.setID(pointerID);
                    prevHoveredEvent.setReleaseOnHoverLeave(releaseOnHoverLeave);
                    emit pointerManager->hoverEndEntity(_prevHoveredObject.objectID, prevHoveredEvent);
                    emit pointerManager->hoverBeginEntity(hoveredObject.objectID, hoveredEvent);
                }
            } else {
                emit pointerManager->hoverBeginEntity(hoveredObject.objectID, hoveredEvent);
                if (_prevHoveredObject.type == OVERLAY) {
                    emit pointerManager->hoverEndOverlay(_prevHoveredObject.objectID, hoveredEvent);
                } else if (_prevHoveredObject.type == HUD) {
                    emit pointerManager->hoverEndHUD(hoveredEvent);
                }
            }
        }

        if (hoveredObject.type == HUD) {
            if (_prevHoveredObject.type == HUD) {
                // There's only one HUD
                emit pointerManager->hoverContinueHUD(hoveredEvent);
            } else {
                emit pointerManager->hoverBeginHUD(hoveredEvent);
                if (_prevHoveredObject.type == ENTITY) {
                    emit pointerManager->hoverEndEntity(_prevHoveredObject.objectID, hoveredEvent);
                } else if (_prevHoveredObject.type == OVERLAY) {
                    emit pointerManager->hoverEndOverlay(_prevHoveredObject.objectID, hoveredEvent);
                }
            }
        }
    }

    if (_hover) {
        // send hoverEnd events if we disable the pointer, disable hovering, or actually stop hovering over an object
        if ((!_enabled && _prevEnabled) || (!doHover && _prevDoHover) || (hoveredObject.type == NONE && _prevHoveredObject.type != NONE)) {
            if (_prevHoveredObject.type == ENTITY) {
                emit pointerManager->hoverEndEntity(_prevHoveredObject.objectID, hoveredEvent);
            } else if (_prevHoveredObject.type == OVERLAY) {
                emit pointerManager->hoverEndOverlay(_prevHoveredObject.objectID, hoveredEvent);
            } else if (_prevHoveredObject.type == HUD) {
                emit pointerManager->hoverEndHUD(hoveredEvent);
            }
        }
    }

    // Trigger begin
    const std::string SHOULD_FOCUS_BUTTON = "Focus";
    for (const std::string& button : newButtons) {
        hoveredEvent.setType(PointerEvent::Press);
        hoveredEvent.setButton(chooseButton(button));
        hoveredEvent.setShouldFocus(button == SHOULD_FOCUS_BUTTON);
        if (hoveredObject.type == ENTITY) {
            emit pointerManager->triggerBeginEntity(hoveredObject.objectID, hoveredEvent);
        } else if (hoveredObject.type == OVERLAY) {
            emit pointerManager->triggerBeginOverlay(hoveredObject.objectID, hoveredEvent);
        } else if (hoveredObject.type == HUD) {
            emit pointerManager->triggerBeginHUD(hoveredEvent);
        }
        _triggeredObjects[button] = hoveredObject;
    }

    // Trigger continue
    for (const std::string& button : sameButtons) {
        PointerEvent triggeredEvent = buildPointerEvent(_triggeredObjects[button], pickResult, false);
        triggeredEvent.setID(pointerID);
        triggeredEvent.setType(PointerEvent::Move);
        triggeredEvent.setButton(chooseButton(button));
        if (_triggeredObjects[button].type == ENTITY) {
            emit pointerManager->triggerContinueEntity(_triggeredObjects[button].objectID, triggeredEvent);
        } else if (_triggeredObjects[button].type == OVERLAY) {
            emit pointerManager->triggerContinueOverlay(_triggeredObjects[button].objectID, triggeredEvent);
        } else if (_triggeredObjects[button].type == HUD) {
            emit pointerManager->triggerContinueHUD(triggeredEvent);
        }
    }

    // Trigger end
    for (const std::string& button : _prevButtons) {
        PointerEvent triggeredEvent = buildPointerEvent(_triggeredObjects[button], pickResult, false);
        triggeredEvent.setID(pointerID);
        triggeredEvent.setType(PointerEvent::Release);
        triggeredEvent.setButton(chooseButton(button));
        if (_triggeredObjects[button].type == ENTITY) {
            emit pointerManager->triggerEndEntity(_triggeredObjects[button].objectID, triggeredEvent);
        } else if (_triggeredObjects[button].type == OVERLAY) {
            emit pointerManager->triggerEndOverlay(_triggeredObjects[button].objectID, triggeredEvent);
        } else if (_triggeredObjects[button].type == HUD) {
            emit pointerManager->triggerEndHUD(triggeredEvent);
        }
        _triggeredObjects.erase(button);
    }

    _prevHoveredObject = hoveredObject;
    _prevButtons = buttons;
    _prevEnabled = _enabled;
    _prevDoHover = doHover;
}

PointerEvent::Button Pointer::chooseButton(const std::string& button) {
    const std::string PRIMARY_BUTTON = "Primary";
    const std::string SECONDARY_BUTTON = "Secondary";
    const std::string TERTIARY_BUTTON = "Tertiary";
    if (button == PRIMARY_BUTTON) {
        return PointerEvent::PrimaryButton;
    } else if (button == SECONDARY_BUTTON) {
        return PointerEvent::SecondaryButton;
    } else if (button == TERTIARY_BUTTON) {
        return PointerEvent::TertiaryButton;
    } else {
        return PointerEvent::NoButtons;
    }
}
