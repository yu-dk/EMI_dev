#include "emi.h"

int seedn = 5;
float freq_th = 0.8;
int incre_cutoff = 0; // if use incre_cutoff, less predictions for missing edge 
float MIN_CLUSTER_DENSITY = 0.6f;
float MIN_WEIGHT = 0.8; 
int MIN_GRAPH_SIZE = 3;
string WINSIZE_TYPE = "";
string LEN_TYPE = ""; // "cM" or "7th"
float WINDOW_SIZE = 0;
float LEN_MIN_WEIGHT = 0;
float LEN_MAX_WEIGHT = 0;
string INPUT_FILE = "test.ibd";
string CLUSTER_FILE = "test"; // File name to write cluster output
string FAM_FILE = "test.fam";
ofstream of_log;

vector<string> vertexName;
float cur_pos = 0;  // current window right boundary 
float cur_pos_start = 0;
struct timeval tv;
long expTime=0, newInputTime=0;

long myclock()
{
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000000) + tv.tv_usec;
}

float getRuntime(long* end, long* start)
{
  return (*end - *start);
}

bool process_param( int argc , char * argv[] )
{
	bool pass = true;
	if (string(argv[1]) == "-help") return false;
	INPUT_FILE = argv[1];
	for( int i = 2; i < argc; i++ )
	{
	        if ( string(argv[i]) == "-win" ) { 
		  WINDOW_SIZE = (float)atof( argv[++i] );
		  WINSIZE_TYPE = string(argv[++i]);
		}
		else if ( string(argv[i]) == "-fam" ) { FAM_FILE = argv[++i]; }
		else if ( string(argv[i]) == "-wgt" ) {
		  LEN_TYPE = string(argv[++i]);
		  LEN_MIN_WEIGHT = (float)atof(argv[++i]);
		  LEN_MAX_WEIGHT = (float)atof(argv[++i]); 
		}
		else if ( string(argv[i]) == "-den" ) { MIN_CLUSTER_DENSITY=(float)atof(argv[++i]);}
		else if ( string(argv[i]) == "-min" ) { MIN_GRAPH_SIZE = atoi( argv[++i] ); }
		else if ( string(argv[i]) == "-nex" ) { seedn = atoi( argv[++i] ); } // num of extened clusters
		else if ( string(argv[i]) == "-fqt" ) { freq_th=(float)atof(argv[++i]);} 
		else if ( string(argv[i]) == "-ict" ) { incre_cutoff=atoi(argv[++i]);} 
		else if ( string(argv[i]) == "-ict" ) { incre_cutoff=atoi(argv[++i]);} 
		else CLUSTER_FILE = argv[i];
	}
	if ( WINDOW_SIZE <= 0 ) { cerr << "ERROR: -win " << WINDOW_SIZE << ": must be greater than zero" << endl; pass = false; }
	if (! (WINSIZE_TYPE == "bp" || WINSIZE_TYPE == "cM")) {cerr << "ERROR: -win " << WINSIZE_TYPE << " " << WINDOW_SIZE << ": sliding window size must be in bp or cM unit" << endl; pass = false; }
	if (! (LEN_TYPE == "bp" || LEN_TYPE == "cM" || LEN_TYPE == "7th") ) {cerr << "ERROR: -wgt " << LEN_TYPE << ", currently only support 3 kinds of weighting" << endl; pass = false;}
	if ( MIN_CLUSTER_DENSITY <= 0 || MIN_CLUSTER_DENSITY > 1 ) { cerr << "ERROR: -density " << MIN_CLUSTER_DENSITY << ": must be greater than zero and less than or equal to one" << endl; pass = false; }
	if ( MIN_GRAPH_SIZE <= 2 ) { cerr << "ERROR: -min " << MIN_GRAPH_SIZE << ": must be greater than 1" << endl; pass = false; }
	if (seedn < 5) {cerr << "ERROR: -nex " <<seedn << ": must be greater than 4" << endl; pass = false;}
	if (freq_th < 0.5) {cerr << "ERROR: -fqt " << freq_th << ": must be greater than 0.5" << endl; pass = false;}
	if ( ! pass ) cerr << endl;
	return pass;
}


int main( int argc , char * argv[] )
{
  if ( argc < 2 || !process_param( argc , argv ) )
    {
      cerr << endl << "Usage: "<< endl;
      cerr << argv[0] << "[input seg file] <parameters> [cluster output file]" << endl;
      cerr << "example1 (all the input parameters are required): " << endl;
      cerr << argv[0] << " test.ibd -fam test.fam -wgt bp 2000000 6000000 -win 20000 bp test" << endl;
      cerr << "example2 (more advanced input parameters ): " << endl;
      cerr << argv[0] << " test.ibd -fam test.fam -wgt cM 1 7 -min 3 -win 0.15 cM -den 0.5 test" << endl;
      cerr << "example3 (more advanced input parameters ): " << endl;
      cerr << argv[0] << " test.ibd -fam test.fam -wgt 7th 3 40 -min 3 -win 0.15 cM -den 0.5 test" << endl << endl;

      cerr << "#############################################################################" << endl;
      cerr << "NB: input segment should be sorted by 5th & 6th column. e.g, sort -k 5,1n -k 6,2n" << endl;
      cerr << "e.g, less input_file | sort -k 5,1n -k 6,2n" << endl;
      cerr << "#############################################################################" << endl;
      cerr << "-help\t\tPrint this output" << endl;
      cerr << "-win [VAL] [STRING]\tSliding window size (VAL) and scale (STRING = bp or cM)" << endl;
      cerr << "-wgt [STRING] [VAL1] [VAL2]\t Weight the input pairwise IBD (STRING) by physical distance (bp), genetic distance (cM) or 7th column of input file(7th) and the value corresponding to the min weight of 0.8 (VAL1) and max weight of 1 (VAL2) " << endl;
      cerr << "-fam [STRING]\tFilename for list of sample identifies in PLINK format" << endl;
      cerr << "-den [VAL]\tMinimum cluster density (default: "<< MIN_CLUSTER_DENSITY << ")" << endl;
      cerr << "-min [VAL]\tMinimum cluster size (default: " << MIN_GRAPH_SIZE << ")" << endl;   
      return -1;
    }

  of_log.open((CLUSTER_FILE + ".log").c_str());
  of_log << "Sliding window size: " << WINDOW_SIZE << " " << WINSIZE_TYPE <<  endl;
  of_log << "input file for pairwise IBD segments: " << INPUT_FILE << endl;
  of_log << "weight the input pairwise IBD by: " << LEN_TYPE << endl;
  of_log << "input segments shorter than " << LEN_MIN_WEIGHT << " "<< LEN_TYPE<< " are discarded" << endl;
  of_log << "input segments longer than " << LEN_MAX_WEIGHT << " " << LEN_TYPE<< " have score of 1 " << endl;
  of_log << "fam file for sample identifies: " << FAM_FILE << endl;
  of_log << "Minimum cluster density: "<< MIN_CLUSTER_DENSITY << endl;
  of_log << "Minimum haplotype size: " << MIN_GRAPH_SIZE << endl;
  of_log << "Output clusters in file: " << CLUSTER_FILE << ".clst.tmp"<< endl;

  // parameter sets
  CmdOpt cmdopt;
  cmdopt.len_type = LEN_TYPE;
  cmdopt.winsize_type = WINSIZE_TYPE;
  cmdopt.window_size = WINDOW_SIZE;
  cmdopt.min_weight = MIN_WEIGHT;
  cmdopt.dist2Weight_a = (1 - MIN_WEIGHT)/(LEN_MAX_WEIGHT-LEN_MIN_WEIGHT); 
  cmdopt.dist2Weight_b = 1 - cmdopt.dist2Weight_a * LEN_MAX_WEIGHT;   
  cmdopt.iter_count = 1;
  cmdopt.window_size_nfold = 2.; // at least that long to be printed (as missing pairs)

  float n_overhead = seedn * 0.05 + 4.5; // [5,10] <==> [10, 110]
  cerr << "n_overhead " << n_overhead << endl;
  
  // read fam files
  map<string,int> vertexNameMap;
  int m_nVertex;
  if (!read_fam_file(FAM_FILE, m_nVertex, vertexNameMap, vertexName)){
    cerr << "can not open input file, "<< FAM_FILE << endl;
    exit(0);
  } 
  of_log << m_nVertex/2 << " individual read from FAM file" << endl;

  // read seg, equal weight
  list<Pairmatch * > matches, active_matches;
  if (!read_beagle_input(INPUT_FILE, matches, vertexNameMap, cmdopt)){ 
    cerr << "can not open input file, "<< INPUT_FILE << endl;
    exit(0);
  } 
  vertexNameMap.clear();
  //cerr << endl << "Done with loading IBD shared segments" << endl;  

  long start = myclock();
  list< Pairmatch * >::iterator pm_iter, am;
  EdgeInfo * p_edge;
  FastGraphCluster cluster(MIN_CLUSTER_DENSITY, MIN_GRAPH_SIZE, MIN_CLUSTER_DENSITY-0.1, m_nVertex);
  //  if (WINSIZE_TYPE == "bp") {
  //    cluster.continuous_empty_wins =  600000/WINDOW_SIZE; 
  //  } else if (WINSIZE_TYPE == "cM"){
  //    cluster.continuous_empty_wins =  0.6/WINDOW_SIZE ;  
  //  }
  cluster.continuous_empty_wins = cmdopt.window_size_nfold;
  cerr << "continuous_empty_wins " << cluster.continuous_empty_wins << endl;  
  
  
  ofstream fout1((CLUSTER_FILE + ".clst.tmp" + intToString(cmdopt.iter_count)).c_str());
  float min_end;
  vector< pair <int, int > > delEdge;
  vector< pair <int, int > > addEdge;
  for (pm_iter = matches.begin(); pm_iter != matches.end() || !active_matches.empty(); ) {
    if (pm_iter != matches.end()) {
      while ( cur_pos < (*pm_iter)->pcm_start + cmdopt.window_size ) cur_pos += cmdopt.window_size;
         
           for (am = active_matches.begin(); am != active_matches.end(); ) {
	   if ( (*am)->pcm_end < cur_pos )
	     {
	       delete cluster.m_neighbor[(*am)->i1][(*am)->i2]; // do NOT delete twice !!
	       cluster.m_neighbor[(*am)->i1].erase((*am)->i2);
	       cluster.m_neighbor[(*am)->i2].erase((*am)->i1);	       
	       delEdge.push_back(make_pair((*am)->i1,(*am)->i2));
	       delete *am;
	       active_matches.erase( am++ );
	     } else am++;
	 }
         
    while ( pm_iter!= matches.end() && (*pm_iter)->pcm_start <= (cur_pos - WINDOW_SIZE) ) {
        if ( (*pm_iter)->pcm_end >= cur_pos ) {
	       active_matches.push_back( *pm_iter );
	       addEdge.push_back(make_pair((*pm_iter)->i1,(*pm_iter)->i2));
	       p_edge = new EdgeInfo((*pm_iter)->weight, (*pm_iter)->p_start, (*pm_iter)->p_end);
	       cluster.m_neighbor[(*pm_iter)->i1][(*pm_iter)->i2] = p_edge;
	       cluster.m_neighbor[(*pm_iter)->i2][(*pm_iter)->i1] = p_edge;
	     } else delete *pm_iter; 	  // match does not take up an entire window
	   matches.erase( pm_iter++ );
	 }
    }

    else {
        min_end = 0;
        for (am = active_matches.begin(); am != active_matches.end(); am++ )
            if ( am == active_matches.begin() || (*am)->pcm_end < min_end ) min_end = (*am)->pcm_end;
        while ( min_end >= cur_pos ) cur_pos += WINDOW_SIZE;
        
        for (am = active_matches.begin(); am != active_matches.end(); ) {
            if ( (*am)->pcm_end < cur_pos ) {
                delete cluster.m_neighbor[(*am)->i1][(*am)->i2];
                cluster.m_neighbor[(*am)->i1].erase((*am)->i2);
                cluster.m_neighbor[(*am)->i2].erase((*am)->i1);
                delEdge.push_back(make_pair((*am)->i1,(*am)->i2));
                delete *am;
                active_matches.erase( am++ );
            } else am++;
        }
 }
        
       cluster.dissolve();
       //cluster.dissolve(delEdge, addEdge); 
       delEdge.clear();
       addEdge.clear();
       cluster.updateInput(active_matches);
       cluster.fastClusterCore(seedn, n_overhead, freq_th, cmdopt.window_size, cmdopt.window_size_nfold, fout1); 
       cur_pos_start = cur_pos;
     }

   long end = myclock();
   cerr << "Time elapsed: " << std::setprecision(6) << getRuntime(&end, &start)/1000.0 << " miliseconds" << endl;
   of_log << "Time elapsed: " << std::setprecision(6) << getRuntime(&end, &start)/1000.0 << " miliseconds" << endl;
   of_log.close();
   fout1.close();
   return 0;
}
