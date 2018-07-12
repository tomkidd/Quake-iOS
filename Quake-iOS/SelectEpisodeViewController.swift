//
//  SelectEpisodeViewController.swift
//  Quake
//
//  Created by Tom Kidd on 7/2/18.
//

import UIKit

class SelectEpisodeViewController: UIViewController {

    var selectedGame:QuakeGame!
    var selectedEpisode:Int = 1

    @IBOutlet weak var boxArtImage: UIImageView!
    @IBOutlet weak var episodeList: UITableView!
    
    let quakeEpisodes = ["Episode 1: Dimension of the Doomed",
                         "Episode 2: The Realm of Black Magic",
                         "Episode 3: The Netherworld",
                         "Episode 4: The Elder World",
                         "Episode 5: Dimension of the Past"
    ]
    
    let quakeXP1Episodes = ["Episode 1: Fortress of the Dead",
                            "Episode 2: Dominion of Darkness",
                            "Episode 3: The Rift"
    ]
    
    let quakeXP2Episodes = ["Episode 1: Hell's Fortress",
                            "Episode 2: The Corridors of Time"
    ]
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        switch selectedGame {
        case .Quake:
            boxArtImage.image = UIImage(named: "quake-box")
        case .QuakeXP1:
            boxArtImage.image = UIImage(named: "quake-xp1-box")
        case .QuakeXP2:
            boxArtImage.image = UIImage(named: "quake-xp2-box")
        default: ()
        }
        
        episodeList.register(UITableViewCell.self, forCellReuseIdentifier: "cell")
        
        episodeList.backgroundColor = UIColor.black
        #if TARGET_OS_IPHONE
        episodeList.separatorStyle = .none
        #endif

        // Do any additional setup after loading the view.
    }

    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
    }
    

    // MARK: - Navigation

    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        if segue.identifier == "SelectLevelSegue" {
            (segue.destination as! SelectLevelViewController).selectedEpisode = selectedEpisode
            if selectedEpisode == 4 {
                (segue.destination as! SelectLevelViewController).selectedGame = .QuakeEp5
            } else {
                (segue.destination as! SelectLevelViewController).selectedGame = selectedGame
            }
        }
    }
    
    
    @IBAction func exitToSelectEpisodeController(segue: UIStoryboardSegue) {
    }


}

extension SelectEpisodeViewController: UITableViewDelegate {
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        selectedEpisode = indexPath.row
        performSegue(withIdentifier: "SelectLevelSegue", sender: self)
    }
}

extension SelectEpisodeViewController: UITableViewDataSource {
    
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        switch selectedGame {
        case .Quake:
            return quakeEpisodes.count
        case .QuakeXP1:
            return quakeXP1Episodes.count
        case .QuakeXP2:
            return quakeXP2Episodes.count
        default: ()
        }
        
        return 0
    }
    
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = episodeList.dequeueReusableCell(withIdentifier: "cell", for: indexPath)
        
        cell.backgroundColor = UIColor.black
        cell.textLabel?.textColor = UIColor.white
        cell.textLabel?.font = UIFont(name: "DpQuake", size: (cell.textLabel?.font?.pointSize)!)
       
        switch selectedGame {
        case .Quake:
            cell.textLabel?.text = quakeEpisodes[indexPath.row]
        case .QuakeXP1:
            cell.textLabel?.text = quakeXP1Episodes[indexPath.row]
        case .QuakeXP2:
            cell.textLabel?.text = quakeXP2Episodes[indexPath.row]
        default: ()
        }
        return cell

    }
    

}
