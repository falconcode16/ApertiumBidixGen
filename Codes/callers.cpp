#include "Graph.cpp"
#include "DensityAlgo.cpp"
#include<algorithm>

struct sortByConfDesc{
    bool operator()(const pair<pair<wordData, wordData>, float> &u, pair<pair<wordData, wordData>, float> &v){
        return u.second > v.second;
    }
};

//load the pairs listed in LangData-List into the graph
void runPairs(Graph &G, vector<pair<string, string>> &lI)
{
    string input_file;
    //ofstream fout;
    //fout.open("../Analysis/Tempfile.txt");
    int prev_nodes = 0, prev_edges = 0; //cumalative vertices/edges till now
    for(int i = 0; i < lI.size(); i++)
    {
        string langpair = lI[i].first + "-" + lI[i].second;
        input_file = "../LangData/Data-" + langpair + ".txt";
        //cout << input_file << endl; //output current input file for tracking progress
        //fout << input_file << endl;
        G.loadData(input_file);
        //fout << "Number of vertices: " << G.vertices.size() - prev_nodes << endl; //vertices in this file
        //fout << "Number of edges: " << G.num_edges - prev_edges << endl; //edges in this file
        prev_nodes = G.vertices.size(); prev_edges = G.num_edges; //total number of nodes
        //cout << "done" << endl;
    }

    //experimental printing of translations with different POS
    /*ofstream diffpos; diffpos.open("../Analysis/DiffPOS.txt");
    int cnt = 0;
    for(auto u: G.vertices){
        for(auto vidx: u.adj){
            wordNode v = G.vertices[vidx];
            if(u.rep.pos != v.rep.pos){
                diffpos << u.rep.surface << " " << v.rep.surface << endl;
                if(u.rep.pos=="noun" && v.rep.pos == "properNoun"){
                    cnt++;
                }
            }
        }
    }
    cout << cnt << endl;*/
}

//get predictions for a single language-pair
void predByLang(string &file_pref, map<string, Graph> &pred,
                string &lp1, string &lp2, map<pair<wordData, wordData>, float> &entries){
    ofstream summary; summary.open(file_pref + "pred-summary.txt");

    for(auto &langpair: pred){
        if(langpair.first!=lp1 && langpair.first!=lp2) continue; //if not required pair continue
        Graph &langG = langpair.second; //graph of the language pair
        summary << langpair.first << endl;
        summary << "Number of vertices: " << langG.vertices.size() << endl;
        summary << "Number of edges: " << langG.num_edges << endl;
        string file_name = file_pref + "predictions.txt";
        ofstream outfile; outfile.open(file_name);
        langG.printGraph(outfile);
        summary << endl; //blank line
    }
    //get possibilities with confidence printed
    vector< pair< pair<wordData, wordData>, float> > ventry(entries.begin(), entries.end());
    sort(ventry.begin(), ventry.end(), sortByConfDesc());
    cout << entries.size() << endl;
    ofstream poss; poss.open(file_pref + "possibilities.txt");
    for(auto &entrypair: ventry){
        //only one of {w1, w2} or {w2, w1} can exist as in implementation
        wordData w1 = entrypair.first.first, w2 = entrypair.first.second;
        if(w1.lang + "-" + w2.lang != lp1) swap(w1, w2);
        if(w1.lang + "-" + w2.lang != lp1) continue;
        poss << w1.surface << " " << w2.surface << " " << entrypair.second << "\n";
        poss << w2.surface << " " << w1.surface << " " << entrypair.second << "\n";
    }
}

//Load a small word based context graph - FIX FUNCTION OR REMOVE
/*void runWords(Graph &G, string &word)
{
    string fin_name = "../SampleWord/" + word + ".txt";
    ofstream fout;
    fout.open("../Results/" + word + "_analysis.txt");
    G.loadData(fin_name, fout); //unidirectional data load
    cout << "loaded" << endl;
}*/

//Run after precomputing biconnected components
int runBicomp(Graph &G, vector<Config> configlist, map<string, int> &POS_to_config, string &prefix,
              map<string, Graph> &pred, string &exptno, string &lp1, string &lp2, InfoSets reqd)
{
    map<pair<wordData, wordData>, float> entries;
    /*string fileout_name = "../Results/" + prefix + "bicomp_out.txt";
    ofstream fout;
    fout.open(fileout_name); fout.close();*/
    int new_trans=0;

    Biconnected B; //object of biconnected computation class
    B.findBicomps(G);


    for(auto SG: G.subGraphs) //iterate over components and run density algo for each
    {
        DensityAlgo D = DensityAlgo(SG, configlist, POS_to_config);
        new_trans += D.run(pred, reqd, entries); //append output to fileout_name
    }

    bool req_bicompless_run = false; //is a cycle-less (transitive) run required?
    InfoSets reqd_transitive = reqd; //additional restrictions for cycle-less run if any
    vector<Config> configlist_transitive = configlist;
    Graph tempG;
    DensityAlgo temp = DensityAlgo(tempG, configlist, POS_to_config);
    for(auto &w: G.vertices){ //iterate over vertices
        if(!temp.wordIsReq(w, reqd)) continue; //if vertex is not required by original reqd ignore
        if(configlist[POS_to_config[w.rep.pos]].transitive==2){ //otherwise if it's a non-bicomp transitive run POS
            req_bicompless_run = true; //then we need a bicompless run
            configlist_transitive[POS_to_config[w.rep.pos]].transitive=1; //set its trans_configlist to 1
            reqd_transitive.condOR["pos"].insert(w.rep.pos); //this POS is to be processed in the transitive run
        }
    }
    if(req_bicompless_run){ //if the bicconnected component-less run is required
        //initialize a transitive Density algo object
        DensityAlgo DTrans = DensityAlgo(G, configlist_transitive, POS_to_config);
        new_trans += DTrans.run(pred, reqd_transitive, entries);
    }

    string pred_file_name = "../Results/Expts/" + exptno + "/Analysis/" + lp1 + "/";
    predByLang(pred_file_name, pred, lp1, lp2, entries);
    return new_trans;
}