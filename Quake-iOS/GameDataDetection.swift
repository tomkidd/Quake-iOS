//
//  GameDataDetection.swift
//  Quake-iOS
//
//  Created by Heriberto Delgado on 7/9/16.
//
//

import UIKit

class GameDataDetection: NSObject
{
    static var sharewarePresent = false

    static var registeredPresent = false
    
    static var missionPack1Present = false
    
    static var missionPack2Present = false
    
    static func detect()
    {
        let documentsDir = Bundle.main.resourcePath!
        
        let id1pak0Attributes = try? FileManager.default.attributesOfItem(atPath: "\(documentsDir)/id1/pak0.pak") as NSDictionary
        let id1pak1Attributes = try? FileManager.default.attributesOfItem(atPath: "\(documentsDir)/id1/pak1.pak") as NSDictionary
        
        let hipnoticpak0Attributes = try? FileManager.default.attributesOfItem(atPath: "\(documentsDir)/hipnotic/pak0.pak") as NSDictionary
        
        let roguepak0Attributes = try? FileManager.default.attributesOfItem(atPath: "\(documentsDir)/rogue/pak0.pak") as NSDictionary
        
        sharewarePresent = false
        registeredPresent = false
        missionPack1Present = false
        missionPack2Present = false
        
        if id1pak0Attributes != nil && id1pak0Attributes!.fileSize() == 18689235
        {
            if id1pak1Attributes != nil && id1pak1Attributes!.fileSize() == 34257856
            {
                registeredPresent = true
                
                if hipnoticpak0Attributes != nil && hipnoticpak0Attributes!.fileSize() == 35527205
                {
                    missionPack1Present = true
                }
                
                if roguepak0Attributes != nil && roguepak0Attributes!.fileSize() == 37875279
                {
                    missionPack2Present = true
                }
            }
            else
            {
                sharewarePresent = true
            }
        }
    }
}
