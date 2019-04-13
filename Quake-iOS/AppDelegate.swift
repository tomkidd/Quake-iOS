//
//  AppDelegate.swift
//  Quake-iOS
//
//  Created by Heriberto Delgado on 5/2/16.
//
//

import UIKit
import GameController

@UIApplicationMain
class AppDelegate: UIResponder, UIApplicationDelegate
{
    var window: UIWindow?

    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool
    {
        NotificationCenter.default.addObserver(self, selector: #selector(controllerDidConnect), name: NSNotification.Name(rawValue: "GCControllerDidConnectNotification"), object: nil)
        
        NotificationCenter.default.addObserver(self, selector: #selector(controllerDidDisconnect), name: NSNotification.Name(rawValue: "GCControllerDidDisconnectNotification"), object: nil)

        
        return true
    }

    func applicationWillResignActive(_ application: UIApplication)
    {
        if startGame
        {
            gameInterrupted = true
            
            if (host_initialized != qboolean(0))
            {
                Host_Shutdown()
            }
            
            sys_ended = qboolean(1)
        }
    }

    func applicationDidEnterBackground(_ application: UIApplication)
    {
    }

    func applicationWillEnterForeground(_ application: UIApplication)
    {
    }

    func applicationDidBecomeActive(_ application: UIApplication)
    {
    }

    func applicationWillTerminate(_ application: UIApplication)
    {
    }
    
    @objc func controllerDidConnect(_ notification: Notification)
    {
        GameControllerSetup.connect(notification.object as! GCController?)
    }
    
    @objc func controllerDidDisconnect(_ notification: Notification)
    {
        GameControllerSetup.disconnect(notification.object as! GCController?)
    }

}

