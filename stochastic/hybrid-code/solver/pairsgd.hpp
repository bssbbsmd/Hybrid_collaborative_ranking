#ifndef __PAIRSGD_HPP__
#define __PAIRSGD_HPP__

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <vector>

#include "../elements.hpp"
#include "../model.hpp"
#include "../ratings.hpp"
#include "../loss.hpp"
#include "../problem.hpp"
#include "../evaluator.hpp"
#include "solver.hpp"

using namespace std;

class SolverPairSGD : public Solver{
	protected:
		double alpha, beta; // this is a modified version of parallal SGD and alpha is set as the 

		vector<int> n_comps_by_user, n_comps_by_item;

		bool sgd_step(Model&, const comparison&, loss_option_t, double, double);

	public:
		SolverPairSGD(): Solver(){}
		SolverPairSGD(double alp, double bet, init_option_t init, int n_th, int m_it = 0):
		       Solver(init, m_it, n_th), alpha(alp), beta(bet) {}

		void solve(Problem&, Model&, Evaluator* eval); 	
};


// lambda: the regularization parameter;
// step_size: the learning rate;
bool SolverPairSGD::sgd_step(Model& model, const comparison& comp, loss_option_t loss_option, double lambda, double step_size) {
	double* user_vec  = &(model.U[comp.user_id * model.rank]);
	double* item1_vec = &(model.V[comp.item1_id * model.rank]);
	double* item2_vec = &(model.V[comp.item2_id * model.rank]);

	int n_comps_user  = n_comps_by_user[comp.user_id] ;
	int n_comps_item1 = n_comps_by_item[comp.item1_id];
	int n_comps_item2 = n_comps_by_item[comp.item2_id];


	if((n_comps_user < 1) || (n_comps_item1) < 1 || (n_comps_item2) < 1)  cout << "Error" << endl;
	
	if(comp.item1_id > model.n_items || comp.item2_id > model.n_items) printf(" Items id exceeds the maximum number of items!!\n");	

	double prod = 0.;
	for(int k=0; k < model.rank; k++) 
		prod += user_vec[k] * comp.comp * (item1_vec[k] - item2_vec[k]);

	if(prod != prod){ 
		return false;  // numerical problems with the value prod;
	}
	//calculate the gradient of the pairwise loss
	double grad_loss = 0.;
	int loss = 0;
	switch(loss){
		case 0:
			grad_loss = -1. / (1. + exp(prod) );
			break;
		default:
			cout << "Error: you must choose a loss function!" << endl;	
	}
	
	//calculate the gradient loss function with regularization term
	if(grad_loss != 0.){
		for( int k =0; k < model.rank; ++k){
			double user_dir = step_size * (grad_loss * comp.comp * (item1_vec[k] - item2_vec[k]) + lambda / (double) n_comps_user * user_vec[k] );
			double item1_dir = step_size * (grad_loss * comp.comp * user_vec[k] + lambda / (double)n_comps_item1 * item1_vec[k]);
			double item2_dir = step_size * (grad_loss * -comp.comp * user_vec[k] + lambda / (double)n_comps_item2* item2_vec[k]);

			//update the parameters;
			user_vec[k] -= user_dir;
			item1_vec[k] -= item1_dir;
			item2_vec[k] -= item2_dir;
		}
	}

//	delete user_vec;
//	delete item1_vec;
//	delete item2_vec;

	return true;	
}

void SolverPairSGD::solve(Problem& prob, Model& model, Evaluator* eval){
	n_users = prob.n_users;
	n_items = prob.n_items;
	n_train_comps = prob.n_train_comps;

	n_comps_by_user.resize(n_users,0);
  	n_comps_by_item.resize(n_items,0);

 	for(int i=0; i<n_train_comps; ++i) {
    		++n_comps_by_user[prob.train[i].user_id];
    		++n_comps_by_item[prob.train[i].item1_id];
    		++n_comps_by_item[prob.train[i].item2_id];
  	} 
	
	double time = omp_get_wtime();
	initialize(prob, model, init_option);
	time = omp_get_wtime() - time;
	
	cout << "Initialization time: " << time << endl;

	bool flag = false;

	double step_size = alpha;

	for(int iter=0; iter < max_iter; ++ iter){
		double time_single_iter = omp_get_wtime();
		// sequencially input the comparison pairs
		for(int idx = 0; idx < n_train_comps; ++idx){
			if(!sgd_step(model, prob.train[idx], prob.loss_option, prob.lambda, step_size)){
				flag = true;
				cout << "numerical error" << endl;
				break;
			}		
		}
	
		if(flag) break;
		
		time = time + (omp_get_wtime() - time_single_iter);
		cout << "Iter=" << iter+1 << " timer=" << time <<" ";

		double f = prob.evaluate(model);

		eval->evaluate(model);
		printf("\n");		
	}
}







































#endif
