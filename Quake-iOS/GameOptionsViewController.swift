//
//  GameOptionsViewController.swift
//  Quake
//
//  Created by Tom Kidd on 7/2/18.
//

import UIKit

class GameOptionsViewController: UIViewController {
    
    @IBOutlet weak var boxArtImage: UIImageView!
    @IBOutlet weak var gameNameLabel: UILabel!
    @IBOutlet weak var subtitleLabel: UILabel!
    @IBOutlet weak var developerNameLabel: UILabel!
    @IBOutlet weak var yearLabel: UILabel!
    
    @IBOutlet weak var startGameButton: UIButton!
    

    var selectedGame:QuakeGame!
    var commandLine = ""
    var demoCommandLine = ""

    override func viewDidLoad() {
        super.viewDidLoad()
        
        switch selectedGame {
        case .Quake:
            boxArtImage.image = UIImage(named: "quake-box")
            gameNameLabel.text = "Quake"
            subtitleLabel.isHidden = true
            developerNameLabel.text = "id Software"
            yearLabel.text = "June 22, 1996"
            commandLine = "+map start"
            demoCommandLine = "demos"
        case .QuakeXP1:
            boxArtImage.image = UIImage(named: "quake-xp1-box")
            gameNameLabel.text = "Quake Mission Pack No. 1"
            subtitleLabel.text = "Scourge of Armagon"
            commandLine = "-game hipnotic -hipnotic +map start"
            demoCommandLine = "-game hipnotic -hipnotic demos"
            developerNameLabel.text = "Hipnotic Interactive"
            yearLabel.text = "February 28, 1997"
        case .QuakeXP2:
            boxArtImage.image = UIImage(named: "quake-xp2-box")
            gameNameLabel.text = "Quake Mission Pack No. 2"
            subtitleLabel.text = "Dissolution of Eternity"
            commandLine = "-rogue +map start"
            demoCommandLine = "-rogue demos"
            developerNameLabel.text = "Rogue Entertainment"
            yearLabel.text = "March 31, 1997"
        default: ()
        }

        // Do any additional setup after loading the view.
    }

    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
    }
    

    // MARK: - Navigation

    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        if segue.identifier == "StartGameSegue" {
            (segue.destination as! GameViewController).additionalCommandLine = commandLine
            (segue.destination as! GameViewController).selectedGame = selectedGame
        } else if segue.identifier == "SelectEpisodeSegue" {
            (segue.destination as! SelectEpisodeViewController).selectedGame = selectedGame
        } else if segue.identifier == "PlayDemoSegue" {
            (segue.destination as! GameViewController).additionalCommandLine = demoCommandLine
            (segue.destination as! GameViewController).demoMode = true
            (segue.destination as! GameViewController).selectedGame = selectedGame
        }
    }
    
    @IBAction func exitToGameOptionsController(segue: UIStoryboardSegue) {
    }


}
