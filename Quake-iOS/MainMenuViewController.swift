//
//  MainMenuViewController.swift
//  Quake
//
//  Created by Tom Kidd on 6/27/18.
//

import UIKit

class MainMenuViewController: UIViewController {
    
    var selectedGame: QuakeGame!

    override func viewDidLoad() {
        super.viewDidLoad()
        
        let defaults = UserDefaults()
        selectedGame = QuakeGame(rawValue: defaults.integer(forKey: "selectedGame"))

        // Do any additional setup after loading the view.
    }

    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
    }
    

    // MARK: - Navigation

    // In a storyboard-based application, you will often want to do a little preparation before navigation
    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        if segue.identifier == "DemoSegue" {
            (segue.destination as! GameViewController).additionalCommandLine = " demos"
        } else if segue.identifier == "StartGameSegue" {
            (segue.destination as! GameViewController).additionalCommandLine = " +map start"
            (segue.destination as! GameViewController).selectedGame = .Quake
        } else if segue.identifier == "ResumeGameSegue" {
            (segue.destination as! GameViewController).selectedGame = selectedGame

            switch selectedGame {
            case .Quake:
                (segue.destination as! GameViewController).additionalCommandLine = "+map start"
            case .QuakeXP1:
                (segue.destination as! GameViewController).additionalCommandLine = "-game hipnotic -hipnotic +map start"
            case .QuakeXP2:
                (segue.destination as! GameViewController).additionalCommandLine = "-rogue +map start"
            default: ()
            }
            
            (segue.destination as! GameViewController).quickLoadGame = true
        }
    }

}
