//
//  ChooseGameViewController.swift
//  Quake
//
//  Created by Tom Kidd on 6/28/18.
//

import UIKit

class ChooseGameViewController: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()

        // Do any additional setup after loading the view.
    }
    
    override func viewDidAppear(_ animated: Bool) {
        self.navigationController?.setNavigationBarHidden(true, animated: false)
    }

    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
    }
    

    // MARK: - Navigation

    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        let defaults = UserDefaults()
        if segue.identifier == "quakeSegue" {
            (segue.destination as! GameOptionsViewController).selectedGame = .Quake
            defaults.set(QuakeGame.Quake.rawValue, forKey: "selectedGame")
        } else if segue.identifier == "quakeXP1Segue" {
            (segue.destination as! GameOptionsViewController).selectedGame = .QuakeXP1
            defaults.set(QuakeGame.QuakeXP1.rawValue, forKey: "selectedGame")
        } else if segue.identifier == "quakeXP2Segue" {
            (segue.destination as! GameOptionsViewController).selectedGame = .QuakeXP2
            defaults.set(QuakeGame.QuakeXP2.rawValue, forKey: "selectedGame")
        }
    }
    
    @IBAction func exitToChooseGameController(segue: UIStoryboardSegue) {
    }

}
