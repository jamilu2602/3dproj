#include "point2mesh.h"

//Constructor
Point2Mesh::Point2Mesh(QObject *parent) : QObject(parent)
{
}

//Function to calculate normal
void Point2Mesh::calc_normal(pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud,
                              pcl::PointCloud<pcl::Normal>::Ptr pc_normals)
{
    pcl::NormalEstimation<pcl::PointXYZRGBA, pcl::Normal> normal_est;
    pcl::search::KdTree<pcl::PointXYZRGBA>::Ptr kdtree (new pcl::search::KdTree<pcl::PointXYZRGBA> ());
    normal_est.setSearchMethod (kdtree);
    normal_est.setKSearch (30);

    normal_est.setInputCloud(cloud);
    normal_est.compute (*pc_normals);
}

//Function to calculate normal point cloud
void Point2Mesh::calc_normals(pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud,
                              pcl::PointCloud<pcl::PointNormal>::Ptr pc_normals)
{
    pcl::NormalEstimation<pcl::PointXYZRGBA, pcl::PointNormal> normal_est;
    pcl::search::KdTree<pcl::PointXYZRGBA>::Ptr kdtree (new pcl::search::KdTree<pcl::PointXYZRGBA> ());
    normal_est.setSearchMethod (kdtree);
    normal_est.setKSearch (30);

    normal_est.setInputCloud(cloud);
    normal_est.compute (*pc_normals);
    pcl::copyPointCloud (*cloud, *pc_normals);
}

//Function for filtering image using StatisticalOutlierRemoval
void Point2Mesh::filtering (pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud)
{
    pcl::StatisticalOutlierRemoval<pcl::PointXYZRGBA>::Ptr filter (new pcl::StatisticalOutlierRemoval<pcl::PointXYZRGBA>());
    filter->setInputCloud (cloud);
    filter->setMeanK (10);
    filter->setStddevMulThresh (1.0);
    filter->filter (*cloud);
}

//Function to perform triangulation (Poison/Greedy) into point cloud
void Point2Mesh::point2mesh (pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud_pcl, int type, int depth)
{
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_pcl_xyz (new pcl::PointCloud<pcl::PointXYZ>);
    pcl::PointCloud<pcl::Normal>::Ptr normal (new pcl::PointCloud<pcl::Normal>);
    pcl::PointCloud<pcl::PointNormal>::Ptr normal_pc (new pcl::PointCloud<pcl::PointNormal>);
    pcl::PolygonMesh triangles;
    pcl::search::KdTree<pcl::PointXYZ>::Ptr kdtree (new pcl::search::KdTree<pcl::PointXYZ>);
    pcl::search::KdTree<pcl::PointNormal>::Ptr kdtreeN (new pcl::search::KdTree<pcl::PointNormal>);

    //Convert PointXYZRGBA into PointXYZ
    pcl::copyPointCloud(*cloud_pcl, *cloud_pcl_xyz);

    //Normal estimation
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> normal_est;
    kdtree->setInputCloud(cloud_pcl_xyz);
    normal_est.setInputCloud(cloud_pcl_xyz);
    normal_est.setSearchMethod(kdtree);
    normal_est.setKSearch(20);
    normal_est.compute(*normal);
    pcl::concatenateFields(*cloud_pcl_xyz, *normal, *normal_pc);
    kdtreeN->setInputCloud(normal_pc);

    if (type == 0)
    {
        qDebug() << "Poison";
        pcl::Poisson<pcl::PointNormal> poisson;
        poisson.setDepth(depth);
        poisson.setInputCloud(normal_pc);
        poisson.reconstruct(triangles);
    }

    else if (type == 1)
    {
        qDebug() << "Greedy";
        pcl::GreedyProjectionTriangulation<pcl::PointNormal> greedy;
        greedy.setSearchRadius(0.8);
        greedy.setMu(10);
        greedy.setMaximumNearestNeighbors (100);
        greedy.setMaximumSurfaceAngle (M_PI);
        greedy.setMinimumAngle (M_PI/10);
        greedy.setMaximumAngle(M_PI);
        greedy.setNormalConsistency (false);
        greedy.setInputCloud (normal_pc);
        greedy.setSearchMethod (kdtreeN);
        greedy.reconstruct (triangles);
    }

    std::string str = "tri.stl";
    pcl::io::savePolygonFileSTL(str, triangles, true);

    qDebug() << "FINSHED TRIANGULATION";
}

//Function to generate align between two pairs of point clouds
void Point2Mesh::estimate_align (const pcl::PointCloud<pcl::PointXYZRGBA>::Ptr &cloud1,
                                 const pcl::PointCloud<pcl::PointXYZRGBA>::Ptr &cloud2,
                                 pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud_final,
                                 Eigen::Matrix4f &transf_m,
                                 bool flag)

{

    pcl::PointCloud<pcl::PointXYZRGBA>::Ptr c1 (new pcl::PointCloud<pcl::PointXYZRGBA>);
    pcl::PointCloud<pcl::PointXYZRGBA>::Ptr c2 (new pcl::PointCloud<pcl::PointXYZRGBA>);
    pcl::PointCloud<pcl::PointNormal>::Ptr normal_c1 (new pcl::PointCloud<pcl::PointNormal>);
    pcl::PointCloud<pcl::PointNormal>::Ptr normal_c2 (new pcl::PointCloud<pcl::PointNormal>);
    pcl::VoxelGrid<pcl::PointXYZRGBA> grid;
    pcl::NormalEstimation<pcl::PointXYZRGBA, pcl::PointNormal> normal_est;
    pcl::search::KdTree<pcl::PointXYZRGBA>::Ptr kdtree (new pcl::search::KdTree<pcl::PointXYZRGBA> ());


    if (flag == true)
    {
        grid.setLeafSize (0.001, 0.001, 0.001);
        grid.setInputCloud (cloud1);
        grid.filter (*c1);

        grid.setInputCloud (cloud2);
        grid.filter (*c2);
    }

    else
    {
        c1 = cloud1;
        c2 = cloud2;
    }

    calc_normals(c1, normal_c1);
    calc_normals(c2, normal_c2);

    pcl::IterativeClosestPointNonLinear<pcl::PointNormal, pcl::PointNormal> icp;
    icp.setTransformationEpsilon (1e-8);
    icp.setMaxCorrespondenceDistance (0.2);
    icp.setRANSACOutlierRejectionThreshold(0.03);
    icp.setInputSource (normal_c1);
    icp.setInputTarget (normal_c2);

    Eigen::Matrix4f m_identity = Eigen::Matrix4f::Identity();
    Eigen::Matrix4f previous, trgt;
    pcl::PointCloud<pcl::PointNormal>::Ptr icp_res = normal_c1;
    icp.setMaximumIterations(2);

    for (int i = 0; i < 30; i++)
    {
        normal_c1 = icp_res;
        icp.setInputSource(normal_c1);
        icp.align(*icp_res);


        m_identity = icp.getFinalTransformation() * m_identity;


        if (fabs ((icp.getLastIncrementalTransformation () -
                   previous).sum ()) < icp.getTransformationEpsilon ())
        {
            icp.setMaxCorrespondenceDistance (icp.getMaxCorrespondenceDistance () - 0.001);
        }

        previous = icp.getLastIncrementalTransformation();
    }

    trgt = m_identity.inverse();
    pcl::transformPointCloud(*cloud2, *cloud_final, trgt);

    *cloud_final += *c1;
    transf_m = trgt;
}
