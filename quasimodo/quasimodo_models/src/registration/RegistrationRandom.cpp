#include "registration/RegistrationRandom.h"
#include <iostream>
#include <fstream>
#include <omp.h>
#include <algorithm>

namespace reglib
{

RegistrationRandom::RegistrationRandom(unsigned int steps){
	only_initial_guess		= false;
	visualizationLvl		= 0;
	refinement				= new RegistrationRefinement();

	steprx		= stepry	= steprz	= steps;
	start_rx	= start_ry	= start_rz	= 0;
	stop_rx		= stop_ry	= stop_rz	= 2.0 * M_PI * double(steps)/double(steps+1);

	steptx		= stepty	= steptz	= 1;
	start_tx	= start_ty	= start_tz	= 0;
	stop_tx		= stop_ty	= stop_tz	= 0;

	src_meantype	= 0;
	dst_meantype	= 0;
}

RegistrationRandom::~RegistrationRandom(){
	delete refinement;
}

void RegistrationRandom::setSrc(CloudData * src_){
	src = src_;
	refinement->setSrc(src_);
}
void RegistrationRandom::setDst(CloudData * dst_){
	dst = dst_;
	refinement->setDst(dst_);
}

double getTime(){
	struct timeval start1;
	gettimeofday(&start1, NULL);
	return double(start1.tv_sec+(start1.tv_usec/1000000.0));
}

double transformationdiff(Eigen::Affine3d & A, Eigen::Affine3d & B, double rotationweight){
	Eigen::Affine3d C = A.inverse()*B;
	double r = fabs(1-C(0,0))+fabs(C(0,1))+fabs(C(0,2))  +  fabs(C(1,0))+fabs(1-C(1,1))+fabs(C(1,2))  +  fabs(C(2,0))+fabs(C(2,1))+fabs(1-C(2,2));
	double t = sqrt(C(0,3)*C(0,3)+C(1,3)*C(1,3)+C(2,3)*C(2,3));
	return r*rotationweight+t;
}


bool compareFusionResults (FusionResults i,FusionResults j) { return i.score > j.score; }

bool RegistrationRandom::issame(FusionResults fr1, FusionResults fr2, int stepxsmall){
	Eigen::Matrix4d current_guess = fr1.guess.inverse()*fr2.guess;
	float m00 = current_guess(0,0); float m01 = current_guess(0,1); float m02 = current_guess(0,2); float m03 = current_guess(0,3);
	float m10 = current_guess(1,0); float m11 = current_guess(1,1); float m12 = current_guess(1,2); float m13 = current_guess(1,3);
	float m20 = current_guess(2,0); float m21 = current_guess(2,1); float m22 = current_guess(2,2); float m23 = current_guess(2,3);

	double sum = 0;
	double count = 0;
	unsigned int s_nr_data = src->data.cols();
	for(unsigned int i = 0; i < s_nr_data/stepxsmall; i++){
		float x		= src->data(0,i*stepxsmall);
		float y		= src->data(1,i*stepxsmall);
		float z		= src->data(2,i*stepxsmall);
		float dx	= m00*x + m01*y + m02*z + m03 - x;
		float dy	= m10*x + m11*y + m12*z + m13 - y;
		float dz	= m20*x + m21*y + m22*z + m23 - z;
		sum += sqrt(dx*dx+dy*dy+dz*dz);
		count++;
	}
	double mean = sum/count;
	//printf("mean: %f\n",mean);
	return mean < 20*fr1.stop;
}

double getPscore(double p, double x, double y,double z, double r, pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cloud){
	double score = 0;
	unsigned int nrp = cloud->points.size();
	for(unsigned int i = 0; i < nrp; i++){
		pcl::PointXYZRGBNormal po = cloud->points[i];
		double dx = po.x-x;
		double dy = po.y-y;
		double dz = po.z-z;
		double dist = sqrt(dx*dx+dy*dy+dz*dz);
		score += pow(fabs(r-dist),p);
	}
	return score;
}


double getPsphere(double p, double & x, double & y,double & z, double & r, pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cloud){
	double h = 0.00001;


	double score = getPscore(p,x,y,z,r,cloud);
	double step = 0.01;
	while(step > 0.0001){
		double next_score = getPscore(p,x,y,z,r+step,cloud);
		while(score > next_score){
			score = next_score;
			r = r+step;
			next_score = getPscore(p,x,y,z,r+step,cloud);
		}
		step *= 0.1;
	}



	for(unsigned int it = 0; it < 1000; it++){
		double score_start = score;
        //printf("it: %10.10i -> %10.10f pos: %5.5f %5.5f %5.5f %5.5f d: ",it,score,x,y,z,r);
		double dx = -(getPscore(p,x+h,y,z,r,cloud) - getPscore(p,x-h,y,z,r,cloud))/(2*h);
		double dy = -(getPscore(p,x,y+h,z,r,cloud) - getPscore(p,x,y-h,z,r,cloud))/(2*h);
		double dz = -(getPscore(p,x,y,z+h,r,cloud) - getPscore(p,x,y,z-h,r,cloud))/(2*h);
		double dr = -(getPscore(p,x,y,z,r+h,cloud) - getPscore(p,x,y,z,r-h,cloud))/(2*h);

        //printf("%5.5f %5.5f %5.5f %5.5f\n",dx,dy,dz,dr);

		double step = 0.001;
		while(step > 0.00000001){
			double next_score = getPscore(p,x+step*dx,y+step*dy,z+step*dz,r+step*dr,cloud);
			while(score > next_score){
				score = next_score;
				x = x+step*dx;
				y = y+step*dy;
				z = z+step*dz;
				r = r+step*dr;
				next_score = getPscore(p,x+step*dx,y+step*dy,z+step*dz,r+step*dr,cloud);
			}
			step *= 0.1;
		}
		double ratio = score/score_start;
		if(ratio > 0.999){break;}
	}
	return score;
}

Eigen::Affine3d RegistrationRandom::getMean(CloudData * data, int type){
	unsigned int nr_data = src->data.cols();

	double mean_x = 0;
	double mean_y = 0;
	double mean_z = 0;

	if(type == 0 || type == 2){
		for(unsigned int i = 0; i < nr_data; i++){
			mean_x += data->data(0,i);
			mean_y += data->data(1,i);
			mean_z += data->data(2,i);
		}
		mean_x /= double(nr_data);
		mean_y /= double(nr_data);
		mean_z /= double(nr_data);

		if(type == 2){
			pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZRGBNormal>);
			cloud->points.clear();
			for(unsigned int i = 0; i < nr_data; i++){pcl::PointXYZRGBNormal p;p.x = data->data(0,i)  ;p.y = data->data(1,i);p.z = data->data(2,i);p.b = 0;p.g = 255;p.r = 0;cloud->points.push_back(p);}

			double sphere_r = 0;
			double score = getPsphere(1.0, mean_x,mean_y,mean_z,sphere_r,cloud);
		}
	}else if(type == 1){
		std::vector<double> xvec;
		std::vector<double> yvec;
		std::vector<double> zvec;
		xvec.resize(nr_data);
		yvec.resize(nr_data);
		zvec.resize(nr_data);
		for(unsigned int i = 0; i < nr_data; i++){
			xvec[i] = data->data(0,i);
			yvec[i] = data->data(1,i);
			zvec[i] = data->data(2,i);
		}

		std::sort (xvec.begin(), xvec.end());
		std::sort (yvec.begin(), yvec.end());
		std::sort (zvec.begin(), zvec.end());

		int mid_ind = nr_data/2;

		mean_x = xvec[mid_ind];
		mean_y = xvec[mid_ind];
		mean_z = xvec[mid_ind];
	}

	Eigen::Affine3d mean = Eigen::Affine3d::Identity();
	mean(0,3) = mean_x;
	mean(1,3) = mean_y;
	mean(2,3) = mean_z;
	return mean;
}

FusionResults RegistrationRandom::getTransform(Eigen::MatrixXd guess){
	std::vector< double > rxs;
	std::vector< double > rys;
	std::vector< double > rzs;

	std::vector< double > txs;
	std::vector< double > tys;
	std::vector< double > tzs;

	for(double rx = 0; rx < steprx; rx++){
		for(double ry = 0; ry < stepry; ry++){
			for(double rz = 0; rz < steprz; rz++){
				for(double tx = 0; tx < steptx; tx++){
					for(double ty = 0; ty < stepty; ty++){
						for(double tz = 0; tz < steptz; tz++){
							rxs.push_back(start_rx + rx*(stop_rx-start_rx));
							rys.push_back(start_ry + ry*(stop_ry-start_ry));
							rzs.push_back(start_rz + rz*(stop_rz-start_rz));
							txs.push_back(start_tx + tx*(stop_tx-start_tx));
							tys.push_back(start_ty + ty*(stop_ty-start_ty));
							tzs.push_back(start_tz + tz*(stop_tz-start_tz));
						}
					}
				}
			}
		}
	}

	unsigned int nr_r = steprx*stepry*steprz*steptx*stepty*steptz;
	for(unsigned int r = 0; r < nr_r; r++){
		printf("registering: %i / %i -> R(%5.5f  %5.5f  %5.5f) T(%5.5f  %5.5f  %5.5f)\n",r+1,nr_r,rxs[r],rys[r],rzs[r],txs[r],tys[r],tzs[r]);
	}
exit(0);
	refinement->allow_regularization = true;

	unsigned int s_nr_data = src->data.cols();
/*
	unsigned int d_nr_data = dst->data.cols();
	double s_mean_x = 0;
	double s_mean_y = 0;
	double s_mean_z = 0;

	double d_mean_x = 0;
	double d_mean_y = 0;
	double d_mean_z = 0;

	if(meantype == 0 || meantype == 2){
		for(unsigned int i = 0; i < s_nr_data; i++){
			s_mean_x += src->data(0,i);
			s_mean_y += src->data(1,i);
			s_mean_z += src->data(2,i);
		}
		s_mean_x /= double(s_nr_data);
		s_mean_y /= double(s_nr_data);
		s_mean_z /= double(s_nr_data);

		for(unsigned int i = 0; i < d_nr_data; i++){
			d_mean_x += dst->data(0,i);
			d_mean_y += dst->data(1,i);
			d_mean_z += dst->data(2,i);
		}
		d_mean_x /= double(d_nr_data);
		d_mean_y /= double(d_nr_data);
		d_mean_z /= double(d_nr_data);

		if(meantype == 2){
			pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr scloud (new pcl::PointCloud<pcl::PointXYZRGBNormal>);
			pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr dcloud (new pcl::PointCloud<pcl::PointXYZRGBNormal>);
			scloud->points.clear();
			dcloud->points.clear();
			for(unsigned int i = 0; i < s_nr_data; i++){pcl::PointXYZRGBNormal p;p.x = src->data(0,i)  ;p.y = src->data(1,i);p.z = src->data(2,i);p.b = 0;p.g = 255;p.r = 0;scloud->points.push_back(p);}
			for(unsigned int i = 0; i < d_nr_data; i++){pcl::PointXYZRGBNormal p;p.x = dst->data(0,i)  ;p.y = dst->data(1,i);p.z = dst->data(2,i);p.b = 0;p.g = 255;p.r = 0;dcloud->points.push_back(p);}

			double s_sphere_r = 0;
			double sscore = getPsphere(1.0, s_mean_x,s_mean_y,s_mean_z,s_sphere_r,scloud);

			double d_sphere_r = 0;
			double dscore = getPsphere(1.0, d_mean_x,d_mean_y,d_mean_z,d_sphere_r,dcloud);
		}

	}else if(meantype == 1){
		std::vector<double> s_xvec;
		std::vector<double> s_yvec;
		std::vector<double> s_zvec;
		s_xvec.resize(s_nr_data);
		s_yvec.resize(s_nr_data);
		s_zvec.resize(s_nr_data);
		for(unsigned int i = 0; i < s_nr_data; i++){
			s_xvec[i] = src->data(0,i);
			s_yvec[i] = src->data(1,i);
			s_zvec[i] = src->data(2,i);
		}

		std::sort (s_xvec.begin(), s_xvec.end());
		std::sort (s_yvec.begin(), s_yvec.end());
		std::sort (s_zvec.begin(), s_zvec.end());

		std::vector<double> d_xvec;
		std::vector<double> d_yvec;
		std::vector<double> d_zvec;
		d_xvec.resize(d_nr_data);
		d_yvec.resize(d_nr_data);
		d_zvec.resize(d_nr_data);
		for(unsigned int i = 0; i < d_nr_data; i++){
			d_xvec[i] = dst->data(0,i);
			d_yvec[i] = dst->data(1,i);
			d_zvec[i] = dst->data(2,i);
		}

		std::sort (d_xvec.begin(), d_xvec.end());
		std::sort (d_yvec.begin(), d_yvec.end());
		std::sort (d_zvec.begin(), d_zvec.end());

		int src_mid_ind = s_nr_data/2;
		int dst_mid_ind = d_nr_data/2;

		s_mean_x = s_xvec[src_mid_ind];
		s_mean_y = s_xvec[src_mid_ind];
		s_mean_z = s_xvec[src_mid_ind];

		d_mean_x = d_xvec[dst_mid_ind];
		d_mean_y = d_xvec[dst_mid_ind];
		d_mean_z = d_xvec[dst_mid_ind];
	}

	Eigen::Affine3d Ymean = Eigen::Affine3d::Identity();
	Ymean(0,3) = d_mean_x;
	Ymean(1,3) = d_mean_y;
	Ymean(2,3) = d_mean_z;

	Eigen::Affine3d Xmean = Eigen::Affine3d::Identity();
	Xmean(0,3) = s_mean_x;
	Xmean(1,3) = s_mean_y;
	Xmean(2,3) = s_mean_z;
*/

	Eigen::Affine3d Xmean = getMean(src,src_meantype);
	Eigen::Affine3d Ymean = getMean(dst,dst_meantype);

	double sumtime = 0;
	double sumtimeSum = 0;
	double sumtimeOK = 0;

	refinement->viewer = viewer;
	refinement->visualizationLvl = 0;
	refinement->target_points = 250;
	int stepxsmall = std::max(1,int(s_nr_data)/refinement->target_points);

	std::vector<FusionResults> fr_X;
	fr_X.resize(nr_r);

	//refinement->visualizationLvl = visualizationLvl;
	//#pragma omp parallel for num_threads(8)
	for(unsigned int r = 0; r < nr_r; r++){
		//printf("registering: %i / %i\n",r+1,nr_r);
		double start = getTime();

		double meantime = 999999999999;
		if(sumtimeOK != 0){meantime = sumtimeSum/double(sumtimeOK+1.0);}
		refinement->maxtime = std::min(0.5,3*meantime);
		if(refinement->visualizationLvl > 0){
			refinement->maxtime = 99999;
		}

		Eigen::Affine3d randomrot = Eigen::Affine3d::Identity();
		randomrot =	Eigen::AngleAxisd(rxs[r], Eigen::Vector3d::UnitX()) *
				Eigen::AngleAxisd(rys[r], Eigen::Vector3d::UnitY()) *
				Eigen::AngleAxisd(rzs[r], Eigen::Vector3d::UnitZ());

		Eigen::Affine3d current_guess = Ymean*randomrot*Xmean.inverse();//*Ymean;

		FusionResults fr = refinement->getTransform(current_guess.matrix());
		//fr_X[r] = refinement->getTransform(current_guess.matrix());

#pragma omp critical
		{
			fr_X[r] = fr;

            printf("%5.5i -> %5.5f %5.5f %5.5f -> score: %10.10f\n",r,rxs[r],rys[r],rzs[r],fr.score);
			double stoptime = getTime();
			sumtime += stoptime-start;
			if(!fr_X[r].timeout){
				sumtimeSum += stoptime-start;
				sumtimeOK++;
			}
			//		bool exists2 = false;
			//		for(unsigned int ax = 0; ax < fr_X.size(); ax++){
			//			if(issame(fr, fr_X[ax],stepxsmall)){exists2 = true; break;}
			//		}
			//		if(!exists2){fr_X.push_back(fr);}
		}
	}



	FusionResults fr = FusionResults();
	refinement->allow_regularization = false;

	int tpbef = refinement->target_points;

	int mul = 2;
	for(int tp = 500; tp <= 16000; tp *= 2){
		printf("------------------------");
		std::sort (fr_X.begin(), fr_X.end(), compareFusionResults);
		refinement->target_points = tp;

		unsigned int nr_X = fr_X.size()/mul;
		//#pragma omp parallel for num_threads(8)
		for(unsigned int ax = 0; ax < nr_X; ax++){
			printf("%5.5i score: %10.10f ",ax,fr_X[ax].score);
			fr_X[ax] = refinement->getTransform(fr_X[ax].guess);
			printf("-> score: %10.10f\n",fr_X[ax].score);
		}

		for(unsigned int ax = 0; ax < fr_X.size(); ax++){
			for(unsigned int bx = ax+1; bx < fr_X.size(); bx++){
				if(issame(fr_X[bx], fr_X[ax],stepxsmall)){
					fr_X[bx] = fr_X.back();
					fr_X.pop_back();
					bx--;
				}
			}
		}
		mul *= 2;
	}

	refinement->visualizationLvl = visualizationLvl;
	std::sort (fr_X.begin(), fr_X.end(), compareFusionResults);
	for(unsigned int ax = 0; ax < fr_X.size() && ax < 500; ax++){
		fr.candidates.push_back(fr_X[ax].guess);
		fr.counts.push_back(1);
		fr.scores.push_back(fr_X[ax].score);
	}


	if(visualizationLvl > 0){
		refinement->allow_regularization = true;
		refinement->visualizationLvl = visualizationLvl;
		refinement->target_points = 1000000;
		refinement->maxtime = 10000;
		for(unsigned int ax = 0; ax < fr_X.size() && ax < 5; ax++){
			printf("%i -> %f\n",ax,fr_X[ax].score);
			std::cout << fr_X[ax].guess << std::endl << std::endl;
			refinement->getTransform(fr_X[ax].guess);
		}
		refinement->visualizationLvl = 0;
	}


	refinement->target_points = tpbef;
	return fr;
}

}
