#include "ModelDatabaseRetrieval.h"

using namespace std;



ModelDatabaseRetrieval::ModelDatabaseRetrieval()
{

}

ModelDatabaseRetrieval::~ModelDatabaseRetrieval(){}

void ModelDatabaseRetrieval::add(reglib::Model * model){
	models.push_back(model);
	printf("number of models: %i\n",models.size());
}

bool ModelDatabaseRetrieval::remove(reglib::Model * model){
	for(unsigned int i = 0; i < models.size(); i++){
		if(models[i] == model){
			models[i] = models.back();
			models.pop_back();
			return true;
		}
	}
	return false;
}

std::vector<reglib::Model *> ModelDatabaseRetrieval::search(reglib::Model * model, int number_of_matches){
	std::vector<reglib::Model *> ret;
		for(unsigned int i = 0; i < models.size(); i++){
		if(models[i] != model){
			ret.push_back(models[i]);
		}
		if(ret.size() == number_of_matches){break;}
	}
	return ret;
}
