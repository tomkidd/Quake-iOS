//
//  GameControllerSetup.swift
//  Quake-iOS
//
//  Created by Heriberto Delgado on 7/8/16.
//
//

import GameController

public var remote: GCController? = nil

public var currentViewController: UIViewController? = nil

class GameControllerSetup: NSObject
{
    static func connect(_ controller: GCController?)
    {
        for controller in GCController.controllers()
        {
            if controller.extendedGamepad != nil && remote == nil
            {
                remote = controller
                
                remote!.playerIndex = .index1
                
                remote!.controllerPausedHandler = { (controller: GCController) -> () in
                    
                    Sys_Key_Event(27, qboolean(1)) // K_ESCAPE, true
                    Sys_Key_Event(27, qboolean(0)) // K_ESCAPE, false
                    
                }
                
                remote!.extendedGamepad!.dpad.up.pressedChangedHandler = { (button: GCControllerButtonInput, value: Float, pressed: Bool) -> () in
                    
                    Sys_Key_Event(128, qboolean(pressed ? 1 : 0)) // K_UPARROW, true / false
                }
                
                
                remote!.extendedGamepad!.dpad.left.pressedChangedHandler = { (button: GCControllerButtonInput, value: Float, pressed: Bool) -> () in
                    
                    Sys_Key_Event(130, qboolean(pressed ? 1 : 0)) // K_LEFTARROW, true / false
                    
                }
                
                remote!.extendedGamepad!.dpad.right.pressedChangedHandler = { (button: GCControllerButtonInput, value: Float, pressed: Bool) -> () in
                    
                    Sys_Key_Event(131, qboolean(pressed ? 1 : 0)) // K_RIGHTARROW, true / false
                    
                }
                
                remote!.extendedGamepad!.dpad.down.pressedChangedHandler = { (button: GCControllerButtonInput, value: Float, pressed: Bool) -> () in
                    
                    Sys_Key_Event(129, qboolean(pressed ? 1 : 0)) // K_DOWNARROW, true / false
                    
                }
                
                remote!.extendedGamepad!.buttonA.pressedChangedHandler = { (button: GCControllerButtonInput, value: Float, pressed: Bool) -> () in
                    
//                    if startGame
//                    {
                        Sys_Key_Event(32, qboolean(pressed ? 1 : 0)) // K_ENTER, true / false
//                    }
//                    else if !pressed
//                    {
//                        startGame = true
//
//                        currentViewController!.navigationController!.popToRootViewController(animated: true)
//                    }
                }
                
                remote!.extendedGamepad!.buttonB.pressedChangedHandler = { (button: GCControllerButtonInput, value: Float, pressed: Bool) -> () in
                    
//                    Sys_Key_Event(27, qboolean(pressed ? 1 : 0)) // K_ESCAPE, true / false
                    
                }
                
                remote!.extendedGamepad!.buttonY.pressedChangedHandler = { (button: GCControllerButtonInput, value: Float, pressed: Bool) -> () in
                    
//                    if (!pressed)
//                    {
//                        glvr_mode += 1
//
//                        if (glvr_mode > 2)
//                        {
//                            glvr_mode = 0
//                        }
//
//                        if (glvr_mode == 0)
//                        {
//                            Sys_Con_Printf ("VR Static mode enabled.\n")
//                        }
//                        else if (glvr_mode == 1)
//                        {
//                            Sys_Con_Printf ("VR Head View mode enabled.\n")
//                        }
//                        else
//                        {
//                            Sys_Con_Printf ("VR Head Forward mode enabled.\n")
//                        }
//                    }
                    
                }
                
                remote!.extendedGamepad!.leftThumbstick.xAxis.valueChangedHandler = { (button: GCControllerAxisInput, value: Float) -> () in
                    
                    in_forwardmove = value
                    
                }
                
                remote!.extendedGamepad!.leftThumbstick.yAxis.valueChangedHandler = { (button: GCControllerAxisInput, value: Float) -> () in
                    
                    in_sidestepmove = value
                    
                }
                
                remote!.extendedGamepad!.rightThumbstick.xAxis.valueChangedHandler = { (button: GCControllerAxisInput, value: Float) -> () in
                    
                    if glvr_mode != 2
                    {
                        in_rollangle = value
                    }
                    
                }
                
                remote!.extendedGamepad!.rightThumbstick.yAxis.valueChangedHandler = { (button: GCControllerAxisInput, value: Float) -> () in
                    
                    if glvr_mode == 0
                    {
                        in_pitchangle = -value
                    }
                    
                }
                
                remote!.extendedGamepad!.rightTrigger.pressedChangedHandler = { (button: GCControllerButtonInput, value: Float, pressed: Bool) -> () in
                    
                    Sys_Key_Event(133, qboolean(pressed ? 1 : 0)) // K_CTRL, true / false
                    
                }
                
                remote!.extendedGamepad!.rightShoulder.pressedChangedHandler = { (button: GCControllerButtonInput, value: Float, pressed: Bool) -> () in
                    
                    if pressed
                    {
                        Sys_Cbuf_AddText("impulse 10\n")
                    }
                    
                }
                
                break
            }
        }
    }

    static func disconnect(_ controller: GCController?)
    {
        if remote == controller
        {
            in_forwardmove = 0.0
            in_sidestepmove = 0.0
            in_rollangle = 0.0
            in_pitchangle = 0.0
            
            remote = nil
        }
    }
}
